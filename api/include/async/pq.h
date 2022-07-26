#pragma once

#include <libpq-fe.h>

#include "async/coro.h"
#include "async/libev-event-loop.h"

namespace async::pq {
using raw_result = std::shared_ptr<PGresult>;

struct connection_storage;
class connection;
class connection_pool;
template <typename... Ts>
class typed_result;
class result;

using timestamp = std::chrono::system_clock::time_point;

struct connection_storage {
	FIXED_CLASS(connection_storage)

	PGconn *conn;
	socket_storage sock;

	connection_storage() {}

	~connection_storage();
};

using raw_connection = std::shared_ptr<connection_storage>;

class db_error : public std::runtime_error {
	using runtime_error::runtime_error;
};

class connection {
	ONLY_DEFAULT_MOVABLE_CLASS(connection)

private:
	raw_connection conn;
	connection_pool *pool;
	bool has_active_transaction = false;

	static coro<void> rollback_and_return(raw_connection conn, connection_pool *pool);

public:
	connection(raw_connection conn_, connection_pool *pool_);
	~connection();

	PGconn *get_raw_connection() const noexcept;

	coro<void> transaction();
	coro<void> commit();
	coro<void> rollback();

	template <typename... Params>
	coro<result> exec(const char *command, Params &&...params) const;
};

class connection_pool : public event_source {
	ONLY_DEFAULT_MOVABLE_CLASS(connection_pool)

private:
	friend connection;

	struct impl;
	std::unique_ptr<impl> pimpl;

	void return_connection(raw_connection conn) noexcept;

public:
	static thread_local std::shared_ptr<connection_pool> local;

	using clock = std::chrono::steady_clock;

	struct creation_info {
		std::string_view db_path;
		std::size_t connections = 1;
		clock::duration creation_cooldown;
	};

	connection_pool(const creation_info &info);
	~connection_pool();

	void bind_to_thread() override;
	void on_init() override;
	void on_stop() override;

	coro<connection> get_connection();
};

class result {
private:
	raw_result res;

public:
	result(PGresult *raw);

	std::size_t rows() const;
	std::size_t columns() const;

	template <typename... Ts>
	typed_result<Ts...> as() const {
		return {res};
	}

	/** @deprecated */
	template <typename... Ts>
	auto iter() const {
		return as<Ts...>();
	}

	void expect0() const;

	template <typename... Ts>
	std::tuple<Ts...> expect1() const;
};

template <typename... Ts>
class typed_result {
private:
	const static std::size_t SIZE = sizeof...(Ts);

	raw_result res;
	int oids[SIZE];

public:
	template <std::size_t... Is>
	class iterator {
	private:
		std::size_t i;
		const typed_result<Ts...> *res;

	public:
		iterator(std::size_t i_, const typed_result<Ts...> *res_, std::index_sequence<Is...>)
			: i(i_), res(res_) {}

		std::tuple<Ts...> operator*() const;

		iterator &operator++() {
			++i;
			return *this;
		}

		std::strong_ordering operator<=>(const iterator &) const = default;
	};

	typed_result(raw_result res_);

	auto begin() const {
		return iterator{(std::size_t) 0, this, std::index_sequence_for<Ts...>{}};
	}

	auto end() const {
		return iterator{(std::size_t) PQntuples(res.get()), this, std::index_sequence_for<Ts...>{}};
	}
};

namespace detail {
	/** @private */
	template <typename T>
	struct oid_decoder {
		static bool is_valid(int oid);
		static T get(char *data, int length, int oid);

		static void check_type(int oid, std::size_t i) {
			if (!is_valid(oid)) {
				throw db_error{fmt::format("Oid {} at column {} cannot be converted to {}", oid, i,
										   typeid(T).name())};
			}
		}
	};

	/** @private */
	template <typename T>
	void lower_param(T &&param, const char *&values, std::optional<std::string> &buff, int &length,
					 int &format);

	/** @private */
	template <typename... Params>
	struct params_lowerer {
		static const int SIZE = sizeof...(Params);

		const char *values[SIZE];
		std::optional<std::string> buff[SIZE];
		int lengths[SIZE], formats[SIZE];

		template <std::size_t idx, typename T>
		void apply(T &&param);

		template <std::size_t... Is>
		params_lowerer(std::index_sequence<Is...>, Params &&...params) {
			(apply<Is>(std::forward<Params>(params)), ...);
		}
	};

	/** @private */
	coro<result> exec(connection_storage &conn, const char *command, int size, const char *values[],
					  int lengths[], int formats[]);
}  // namespace detail

#include "detail/pq.impl.h"
}  // namespace async::pq