#include "async/pq.h"

#define _BSD_SOURCE
#include <endian.h>

#include "KEGE.h"
#include "async/future.h"
#include "async/socket.h"
using namespace async;
using namespace pq;

/* ==== async::pq::connection_storage ==== */
connection_storage::~connection_storage() {
	libev_event_loop::get()->socket_del(&sock);
	PQfinish(conn);
}

/* ==== async::pq::connection ==== */
coro<void> connection::rollback_and_return(raw_connection conn, connection_pool *pool) {
	co_await pq::detail::exec(*conn, "ROLLBACK", 0, nullptr, nullptr, nullptr);
	pool->return_connection(conn);
}

connection::connection(raw_connection conn_, connection_pool *pool_) : conn(conn_), pool(pool_) {}

connection::~connection() {
	if (conn) {
		if (!has_active_transaction) {
			pool->return_connection(conn);
		} else {
			async::schedule_detached(connection::rollback_and_return(conn, pool));
		}
	}
}

PGconn *connection::get_raw_connection() const noexcept {
	return conn->conn;
}

coro<void> connection::transaction() {
	if (has_active_transaction) {
		throw db_error("tried to nest db transactions");
	}
	co_await exec("BEGIN");
	has_active_transaction = true;
}

coro<void> connection::commit() {
	if (!has_active_transaction) {
		throw db_error("nothing to commit");
	}
	co_await exec("COMMIT");
	has_active_transaction = false;
}

coro<void> connection::rollback() {
	if (!has_active_transaction) {
		throw db_error("nothing to rollback");
	}
	co_await exec("ROLLBACK");
	has_active_transaction = false;
}

/* ==== async::pq::connection_pool::impl ==== */
/** @private */
struct connection_pool::impl {
	std::string db_path;
	std::size_t conns_desired, conns_alive = 0;
	bool stop_requested = false;

	clock::duration creation_cooldown;
	clock::time_point last_failure;

	std::vector<raw_connection> pool;
	std::queue<future<raw_connection> *> requests;

	raw_connection create_connection() {
		auto conn = PQconnectdb(db_path.c_str());
		if (PQstatus(conn) != CONNECTION_OK || PQsetnonblocking(conn, 1)) {
			PQfinish(conn);
			return {};
		}

		auto storage = std::make_shared<connection_storage>();
		storage->conn = conn;
		storage->sock.fd = PQsocket(conn);
		storage->sock.event_mask = READABLE;
		libev_event_loop::get()->socket_add(&storage->sock);
		++conns_alive;
		return storage;
	}

	auto populate_pool() {
		auto now = clock::now();
		if (now - last_failure < creation_cooldown) {
			return;
		}
		bool had_failure = false;
		for (auto i = conns_alive; i < conns_desired; ++i) {
			auto conn = create_connection();
			if (conn) {
				return_connection(conn, true);
			} else {
				had_failure = true;
			}
		}
		if (had_failure) {
			last_failure = now;
		}
	}

	impl(const creation_info &info)
		: db_path(info.db_path),
		  conns_desired(info.connections),
		  creation_cooldown(info.creation_cooldown) {
		last_failure = clock::now() - creation_cooldown - std::chrono::seconds(1);
	}

	void return_connection(raw_connection conn, bool internal = false) {
		if (stop_requested) {
			return;
		}
		if (!internal && PQstatus(conn->conn) != CONNECTION_OK) {
			--conns_alive;
			logw("Connection to the database was lost");
			populate_pool();
			return;
		}
		if (requests.empty()) {
			pool.push_back(conn);
		} else {
			requests.front()->set_result(conn);
			requests.pop();
		}
	}

	void reject_request(future<raw_connection> *req) {
		req->set_exception(
			std::make_exception_ptr(std::runtime_error("could not connect to the database")));
	}

	coro<connection> get_connection(connection_pool *parent) {
		future<raw_connection> fut;
		if (stop_requested) {
			reject_request(&fut);
		} else {
			while (pool.size() && PQstatus(pool.back()->conn) != CONNECTION_OK) {
				--conns_alive;
				logw("Connection to the database was lost");
				pool.pop_back();
			}
			if (conns_alive != conns_desired) {
				populate_pool();
			}
			if (!conns_alive) {
				reject_request(&fut);
			} else if (pool.empty()) {
				requests.push(&fut);
			} else {
				fut.set_result(pool.back());
				pool.pop_back();
			}
		}
		co_return {co_await fut, parent};
	}

	void on_stop() {
		stop_requested = true;
		pool.clear();
		while (requests.size()) {
			reject_request(requests.front());
			requests.pop();
		}
	}
};

/* ==== async::pq::connection_pool ==== */
thread_local std::shared_ptr<connection_pool> connection_pool::local;

void connection_pool::return_connection(raw_connection conn) noexcept {
	pimpl->return_connection(conn);
}

connection_pool::connection_pool(const creation_info &info) {
	pimpl = std::make_unique<impl>(info);
}

connection_pool::~connection_pool() = default;

void connection_pool::bind_to_thread() {
	local = std::static_pointer_cast<connection_pool, event_source>(shared_from_this());
}

void connection_pool::on_init() {
	pimpl->populate_pool();
	if (pimpl->conns_alive != pimpl->conns_desired) {
		logw("Could not create pool of {} DB connections (only have {})", pimpl->conns_desired,
			 pimpl->conns_alive);
	}
}

void connection_pool::on_stop() {
	pimpl->on_stop();
}

coro<connection> connection_pool::get_connection() {
	return pimpl->get_connection(this);
}

/* ==== async::pq::result ==== */
result::result(PGresult *raw) : res({raw, PQclear}) {
	int status = PQresultStatus(raw);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
		throw db_error{PQresultErrorMessage(raw)};
	}
}

std::size_t result::rows() const {
	return std::size_t(PQntuples(res.get()));
}

std::size_t result::columns() const {
	return std::size_t(PQnfields(res.get()));
}

void result::expect0() const {
	if (rows()) {
		throw db_error{"Expected 0 rows as a result"};
	}
}

/* ==== async::pq::detail ==== */
coro<result> pq::detail::exec(connection_storage &c, const char *command, int size,
							  const char *values[], int lengths[], int formats[]) {
	c.sock.event_mask = SOCK_ALL;
	libev_event_loop::get()->socket_mod(&c.sock);

	if (!PQsendQueryParams(c.conn, command, size, nullptr, values, lengths, formats, 1)) {
		throw pq::db_error(PQerrorMessage(c.conn));
	}

	while (true) {
		int result = PQflush(c.conn);
		assert(result != -1);
		if (result == 0) {
			break;
		}
		auto which = co_await socket_performer{SOCK_ALL, &c.sock};
		if (which & READABLE) {
			// TODO Unsure if connection is usable after PQconsumeInput error, the best option here
			// will be to invalidate and close connection
			assert(PQconsumeInput(c.conn));
		}
	}

	c.sock.event_mask = READABLE;
	libev_event_loop::get()->socket_mod(&c.sock);

	while (PQisBusy(c.conn)) {
		co_await socket_performer{READABLE, &c.sock};
		assert(PQconsumeInput(c.conn));
	}

	PGresult *latest = nullptr;
	while (true) {
		auto curr = PQgetResult(c.conn);
		if (!curr) {
			break;
		} else if (latest) {
			PQclear(latest);
		}
		latest = curr;
	}
	co_return {latest};
}

/* ==== Decoders for PQ binary format ==== */
#define SIMPLE_PQ_DECODER(T, OID)                                          \
	template <>                                                            \
	bool pq::detail::pq_binary_converter<T>::is_valid(int oid) {           \
		return oid == OID;                                                 \
	}                                                                      \
                                                                           \
	template <>                                                            \
	T pq::detail::pq_binary_converter<T>::get([[maybe_unused]] char *data, \
											  [[maybe_unused]] int len, [[maybe_unused]] int oid)

#define SIMPLE_PQ_ENCODER(T)                                                 \
	template <>                                                              \
	std::optional<std::string_view> pq::detail::pq_binary_converter<T>::set( \
		const T &obj, [[maybe_unused]] std::string &buff)

#define DELEGATE_PQ_CONVERSION(FROM, TO)                                            \
	template <>                                                                     \
	bool pq::detail::pq_binary_converter<FROM>::is_valid(int oid) {                 \
		return pq_binary_converter<TO>::is_valid(oid);                              \
	}                                                                               \
                                                                                    \
	template <>                                                                     \
	FROM pq::detail::pq_binary_converter<FROM>::get(char *data, int len, int oid) { \
		return (FROM){pq_binary_converter<TO>::get(data, len, oid)};                \
	}                                                                               \
                                                                                    \
	template <>                                                                     \
	std::optional<std::string_view> pq::detail::pq_binary_converter<FROM>::set(     \
		const FROM &obj, std::string &buff) {                                       \
		return pq_binary_converter<TO>::set((TO){obj}, buff);                       \
	}

SIMPLE_PQ_DECODER(bool, 16) {
	return bool(data[0]);
}

SIMPLE_PQ_ENCODER(bool) {
	static char VALUES[] = {0, 1};
	return std::string_view(VALUES + obj, 1);
}

SIMPLE_PQ_DECODER(int64_t, 20) {
	assert(len == 8);
	uint64_t res;
	memcpy(&res, data, len);
	res = be64toh(res);
	return int64_t(res);
}

SIMPLE_PQ_ENCODER(int64_t) {
	uint64_t res = htobe64(static_cast<uint64_t>(obj));
	buff.append(reinterpret_cast<char *>(&res), 8);
	return {};
}

SIMPLE_PQ_DECODER(int, 23) {
	assert(len == 4);
	uint32_t res;
	memcpy(&res, data, len);
	res = be32toh(res);
	return int(res);
}

SIMPLE_PQ_ENCODER(int) {
	uint32_t res = htobe32(static_cast<uint32_t>(obj));
	buff.append(reinterpret_cast<char *>(&res), 4);
	return {};
}

SIMPLE_PQ_DECODER(double, 701) {
	assert(len == 8);
	double res;
	memcpy(&res, data, len);
	return res;
}

const auto PQ_EPOCH = std::chrono::sys_days{std::chrono::January / 1 / 2000};

SIMPLE_PQ_DECODER(timestamp, 1114) {
	using namespace std::chrono;

#if KEGE_PQ_FP_TIMESTAMP
	auto secs = pq_binary_converter<double>::get(data, len, oid);
	return PQ_EPOCH + round<microseconds>(duration<double>(secs));
#else
	auto secs = pq_binary_converter<int64_t>::get(data, len, oid);
	return PQ_EPOCH + microseconds{secs};
#endif
}

SIMPLE_PQ_ENCODER(timestamp) {
	using namespace std::chrono;

#if KEGE_PQ_FP_TIMESTAMP
	static_assert(false);
#else
	return pq_binary_converter<int64_t>::set(round<microseconds>(obj - PQ_EPOCH).count(), buff);
#endif
}

template <>
bool pq::detail::pq_binary_converter<std::string_view>::is_valid(int oid) {
	return oid == 17 || oid == 25 || oid == 1042;
}

template <>
std::string_view pq::detail::pq_binary_converter<std::string_view>::get(char *data, int len, int) {
	return {data, (unsigned) len};
}

SIMPLE_PQ_ENCODER(std::string_view) {
	return obj;
}

DELEGATE_PQ_CONVERSION(std::string, std::string_view)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
DELEGATE_PQ_CONVERSION(unsigned int, int)
DELEGATE_PQ_CONVERSION(uint64_t, int64_t)
DELEGATE_PQ_CONVERSION(long long, int64_t)
DELEGATE_PQ_CONVERSION(unsigned long long, uint64_t)
#pragma GCC diagnostic pop
