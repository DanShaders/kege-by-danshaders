#include "async/pq.h"

#define _BSD_SOURCE
#include <endian.h>

#include "async/future.h"
#include "async/socket.h"
using namespace async;
using namespace pq;

#include "logging.h"

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
			logging::warn("Connection to the database was lost");
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
				logging::warn("Connection to the database was lost");
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

PGresult *result::get_raw_result() {
	return res.get();
}

std::size_t result::rows() const {
	return std::size_t(PQntuples(res.get()));
}

std::size_t result::columns() const {
	return std::size_t(PQnfields(res.get()));
}

char const *result::operator()(std::size_t i, std::size_t j) const {
	return PQgetvalue(res.get(), int(i), int(j));
}

void result::expect0() const {
	if (rows()) {
		throw db_error{"Expected 0 rows as a result"};
	}
}

/* ==== async::pq::detail ==== */
bool pq_recv_bool(char const *data, int len) {
	assert(len == 1);
	return bool(data[0]);
}

int64_t pq_recv_int64(char const *data, int len) {
	assert(len == 8);
	uint64_t res;
	memcpy(&res, data, len);
	res = be64toh(res);
	return int64_t(res);
}

int pq_recv_int32(char const *data, int len) {
	assert(len == 4);
	uint32_t res;
	memcpy(&res, data, len);
	res = be32toh(res);
	return int(res);
}

std::string_view pq_recv_text(char const *data, int len) {
	return {data, (unsigned) len};
}

double pq_recv_double(char const *data, int len) {
	assert(len == 8);
	double res;
	memcpy(&res, data, len);
	return res;
}

std::vector<std::pair<Oid, void *>> pq::detail::pq_decoder::map = {
	{16, (void *) pq_recv_bool},   {17, (void *) pq_recv_text}, {20, (void *) pq_recv_int64},
	{23, (void *) pq_recv_int32},  {25, (void *) pq_recv_text}, {701, (void *) pq_recv_double},
	{1042, (void *) pq_recv_text},
};

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
