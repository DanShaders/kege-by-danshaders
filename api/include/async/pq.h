#pragma once

#include <libpq-fe.h>

namespace async {
namespace pq {
	using raw_connection = std::shared_ptr<PGconn>;
	class connection;
	class connection_pool;

	class connection {
		ONLY_DEFAULT_MOVABLE_CLASS(connection)

	private:
		raw_connection conn;
		connection_pool *pool;

	public:
		connection(raw_connection conn_, connection_pool *pool_);
		~connection();
	};

	class connection_pool {
		ONLY_DEFAULT_MOVABLE_CLASS(connection_pool)

	private:
		class impl;
		impl *pimpl;

		void return_connection(raw_connection conn) noexcept;

	public:
		connection_pool(std::string_view &conn_info, int connections,
						std::chrono::duration creation_cooldown);

		future<raw_connection> &get_connection() noexcept;
		int total_connections();
		
	};
}  // namespace pq
}  // namespace async