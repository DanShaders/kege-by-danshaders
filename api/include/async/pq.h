#pragma once

#include <libpq-fe.h>

#include "async/coro.h"

namespace async {
namespace pq {
	using raw_connection = std::shared_ptr<PGconn>;
	class connection;
	class connection_pool;
	template <typename... T>
	class iterable_typed_result;
	class result;

	class connection {
		ONLY_DEFAULT_MOVABLE_CLASS(connection)

	private:
		raw_connection conn;
		connection_pool *pool;

	public:
		connection(raw_connection conn_, connection_pool *pool_);
		~connection();

		PGconn *get_raw_connection() const noexcept;

		template <typename... Params>
		coro<result> exec(const char *command, Params &&...params);
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
		std::shared_ptr<PGresult> raw;

	public:

		result(PGresult *raw_);

		PGresult *get_raw_result();
		std::size_t size() const;

		char *get_unchecked(std::size_t i, std::size_t j) const {
			return PQgetvalue(raw.get(), int(i), int(j));
		}

		template <typename... T>
		iterable_typed_result<T...> iter() const;
	};

	template <typename... T>
	class iterable_typed_result {
	private:
		result const *res;

	public:
		template <std::size_t... Is>
		class iterator {
		private:
			std::size_t i;
			result const *res;
		
			template <typename U, std::size_t j>
			U row_value_cast() {
				char *data = res->get_unchecked(i, j);

				if constexpr (std::same_as<U, std::string>) {
					return {data};
				} else if constexpr (std::integral<U>) {
					return U{strtoll(data, nullptr, 10)};
				} else {
					return {};
				}
			}
		public:
			iterator(std::size_t i_, result const *res_, std::index_sequence<Is...>) : i(i_), res(res_) {}
			
			std::tuple<T...> operator*() {
				return {row_value_cast<T, Is>()...};
			}

			iterator &operator++() {
				++i;
				return *this;
			}

			std::strong_ordering operator<=>(const iterator &) const = default;
		};

		iterable_typed_result(result const *res_) : res(res_) {}

		auto begin() {
			return iterator{0, res, std::index_sequence_for<T...>{}};
		}

		auto end() {
			return iterator{res->size(), res, std::index_sequence_for<T...>{}};
		}
	};

	class db_error : public std::runtime_error {
		using runtime_error::runtime_error;
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

		coro<result> exec(PGconn *conn, const char *command, int size, const char *values[],
							  int lengths[], int formats[]);
	}  // namespace detail

#include "detail/pq.impl.h"
}  // namespace pq
}  // namespace async