#pragma once

#include <libpq-fe.h>

#include "async/coro.h"

namespace async {
namespace pq {
	using raw_connection = std::shared_ptr<PGconn>;
	using raw_result = std::shared_ptr<PGresult>;

	class connection;
	class connection_pool;
	template <typename... T>
	class iterable_typed_result;
	class result;

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

		PGresult *get_raw_result();
		std::size_t rows() const;
		std::size_t columns() const;
		char const *operator()(std::size_t i, std::size_t j) const;

		template <typename... T>
		iterable_typed_result<T...> iter() const {
			return {res};
		}

		void expect0() const;
		template <typename... T>
		std::tuple<T...> expect1() const;
	};

	template <typename... T>
	class iterable_typed_result {
	private:
		raw_result res;
		Oid oids[sizeof...(T)];

	public:
		template <std::size_t... Is>
		class iterator {
		private:
			std::size_t i;
			iterable_typed_result<T...> *res;

			template <typename U, std::size_t j>
			U get_cell_value();

		public:
			iterator(std::size_t i_, iterable_typed_result<T...> *res_, std::index_sequence<Is...>)
				: i(i_), res(res_) {}

			std::tuple<T...> operator*() {
				return {get_cell_value<T, Is>()...};
			}

			iterator &operator++() {
				++i;
				return *this;
			}

			std::strong_ordering operator<=>(const iterator &) const = default;
		};

		iterable_typed_result(raw_result res_) : res(res_) {
			if (sizeof...(T) != PQnfields(res.get())) {
				throw db_error{"Unexpected number of columns"};
			}
			for (std::size_t i = 0; i < sizeof...(T); ++i) {
				oids[i] = PQftype(res.get(), int(i));
			}
		}

		auto begin() {
			return iterator{(std::size_t) 0, this, std::index_sequence_for<T...>{}};
		}

		auto end() {
			return iterator{(std::size_t) PQntuples(res.get()), this,
							std::index_sequence_for<T...>{}};
		}
	};

	namespace detail {
		template <typename T>
		void lower_param(T &&param, const char *&values, std::optional<std::string> &buff,
						 int &length, int &format);

		template <typename... Params>
		struct params_lowerer {
			static const int SIZE = sizeof...(Params);

			const char *values[SIZE];
			std::optional<std::string> buff[SIZE];
			int lengths[SIZE], formats[SIZE];

			template <std::size_t idx, typename T>
			void apply(T &&param);

			template <std::size_t... Is>
			params_lowerer(std::index_sequence<Is...>, Params &&...params);
			params_lowerer(Params &&...params);
		};

		struct pq_decoder {
			static std::vector<std::pair<Oid, void *>> map;
		};

		template <typename T>
		constexpr bool is_optional(T const &) {
			return false;
		}
		template <typename T>
		constexpr bool is_optional(std::optional<T> const &) {
			return true;
		}

		template <typename T>
		T get_raw_cell_value(PGresult *res, unsigned *oids, int i, int j);

		coro<result> exec(PGconn *conn, const char *command, int size, const char *values[],
						  int lengths[], int formats[]);
	}  // namespace detail

#include "detail/pq.impl.h"
}  // namespace pq
}  // namespace async