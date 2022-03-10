#include "stdafx.h"
#include "config.h"

#include <signal.h>

#include "KEGE.h"
#include "async/coro.h"
#include "async/curl.h"
#include "async/libev-event-loop.h"
#include "async/pq.h"
#include "logging.h"
#include "routes.h"

coro<void> perform_request(fcgx::request_t *) noexcept;

namespace {
bool should_exit = 0;

void interrupt_handler(int, siginfo_t *, void *) noexcept {
	const char *KILLING_APP_STR = "\nKilling application...\n";
	const char *STOPPING_APP_STR = "\nGracefully stopping... (Press Ctrl+C again to force)\n";

	int saved_errno = errno;
	if (should_exit) {
		write(0, KILLING_APP_STR, strlen(KILLING_APP_STR));
		std::terminate();
	} else {
		write(0, STOPPING_APP_STR, strlen(STOPPING_APP_STR));
		should_exit = 1;
		errno = saved_errno;
	}
}

config::hl_socket_address socket_address_apply_id(config::hl_socket_address addr, std::size_t id) {
	if (!addr.use_unix_sockets) {
		return addr;
	} else {
		auto &s = addr.path;
		if (auto pos = s.find("{id}"); pos != std::string::npos) {
			s = s.substr(0, pos) + std::to_string(id) + s.substr(pos + 4);
		}
		return addr;
	}
}

coro<void> perform_request_wrap(FCGX_Request *raw) {
	auto r = co_await fcgx::from_raw(raw);
	if (r) {
		co_await perform_request(r);
		r->finish();
	}
}
}  // namespace

signed main() {
	std::cout << "'KEGE by DanShaders' API (version: " << KEGE_VERSION << ", " BUILD_TYPE " build)"
			  << std::endl
			  << "Compiled at " __DATE__ " " __TIME__ " with GCC " __VERSION__ << std::endl
			  << std::endl;

	logging::init();
	logging::set_thread_name("main");
	{
		char const *kege_root = getenv("KEGE_ROOT");
		conf = config::find_config(kege_root ? kege_root : ".");
	}
	routes::route_storage::instance()->apply_root(conf.api_root);

	assert(!FCGX_Init());
	assert(!curl_global_init(CURL_GLOBAL_DEFAULT));

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	assert(!sigemptyset(&sa.sa_mask));
	sa.sa_sigaction = interrupt_handler;
	assert(!sigaction(SIGINT, &sa, 0));

	struct {
		std::thread t;
		std::shared_ptr<async::event_loop> loop;
	} workers[conf.request_workers];

	auto worker_func = [&](std::size_t worker_id) {
		auto &data = workers[worker_id];
		logging::set_thread_name("worker-" + std::to_string(worker_id));
		data.loop->bind_to_thread();
		data.loop->run(false);
	};

	async::pq::connection_pool::creation_info pq_creation_info = {
		.db_path = conf.db.path,
		.connections = conf.db.connections,
		.creation_cooldown = std::chrono::milliseconds(conf.db.cooldown)};

	for (std::size_t worker = 0; worker < conf.request_workers; ++worker) {
		auto &data = workers[worker];
		data.loop = std::make_shared<async::libev_event_loop>();
		data.loop->register_source(std::make_shared<async::curl_event_source>());
		data.loop->register_source(std::make_shared<async::pq::connection_pool>(pq_creation_info));
		data.loop->register_source(std::make_shared<fcgx::server>(
			socket_address_apply_id(conf.fastcgi, worker), FCGI_QUEUE_SIZE,
			[](FCGX_Request *raw) { schedule_detached(perform_request_wrap(raw)); }));
	}

	for (std::size_t worker = 0; worker < conf.request_workers; ++worker) {
		workers[worker].t = std::thread(worker_func, worker);
	}

	while (!should_exit) {
		pause();
	}

	for (std::size_t id = 0; id < conf.request_workers; ++id) {
		workers[id].loop->stop_notify();
	}
	logging::info("Waiting for workers to process all remaining requests...");
	for (std::size_t id = 0; id < conf.request_workers; ++id) {
		workers[id].t.join();
	}

	return 0;
}