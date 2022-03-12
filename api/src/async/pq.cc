#include "async/pq.h"

#include "async/future.h"
using namespace async;
using namespace pq;

#include "logging.h"

/* ==== async::pq::connection ==== */
connection::connection(raw_connection conn_, connection_pool *pool_) : conn(conn_), pool(pool_) {}

connection::~connection() {
	if (conn) {
		pool->return_connection(conn);
	}
}

PGconn *connection::get_raw_connection() const noexcept {
	return conn.get();
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

	auto create_connection() {
		raw_connection conn{PQconnectdb(db_path.c_str()), PQfinish};
		if (PQstatus(conn.get()) != CONNECTION_OK) {
			return raw_connection{};
		}
		++conns_alive;
		return conn;
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
		if (!internal && PQstatus(conn.get()) != CONNECTION_OK) {
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
			if (conns_alive != conns_desired) {
				populate_pool();
			}
			if (!conns_alive) {
				reject_request(&fut);
			} else {
				if (pool.empty()) {
					requests.push(&fut);
				} else {
					fut.set_result(pool.back());
					pool.pop_back();
				}
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
coro<result> pq::detail::exec(PGconn *conn, const char *command, int size, const char *values[],
							  int lengths[], int formats[]) {
	PGresult *raw = PQexecParams(conn, command, size, nullptr, values, lengths, formats, 0);
	co_return result{raw};
}