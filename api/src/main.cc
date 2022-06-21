#include "stdafx.h"
#include "config.h"

#include <signal.h>

#include "KEGE.h"
#include "async/coro.h"
#include "async/curl.h"
#include "async/libev-event-loop.h"
#include "async/pq.h"
#include "routes.h"
#include "stacktrace.h"

// routes/_wrap.cc
coro<void> perform_request(fcgx::request_t *) noexcept;

inline std::atomic_bool should_exit = false;

namespace {
void interrupt_handler(int, siginfo_t *, void *) noexcept {
	const char *KILLING_APP_STR = "\nKilling application...\n";
	const char *STOPPING_APP_STR = "\nGracefully stopping... (Press Ctrl+C again to force)\n";

	int saved_errno = errno;
	if (should_exit) {
		write(0, KILLING_APP_STR, strlen(KILLING_APP_STR));
		std::terminate();
	} else {
		write(0, STOPPING_APP_STR, strlen(STOPPING_APP_STR));
		should_exit = true;
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

int main() {
	std::cout << "'KEGE by DanShaders' API (version: " << KEGE_VERSION << ", " BUILD_TYPE " build)"
			  << std::endl
			  << "Compiled at " __DATE__ " " __TIME__ " with GCC " __VERSION__ << std::endl
			  << std::endl;

	stacktrace::init();
	async::detail::on_unhandled_exception_cb = stacktrace::log_unhandled_exception;
	std::set_terminate(stacktrace::terminate_handler);

	fmtlog::setThreadName("main");
	fmtlog::setHeaderPattern("[{YmdHMSe}] [{t}] {l} ");

	{
		char const *kege_root = getenv("KEGE_ROOT");
		conf = config::find_config(kege_root ? kege_root : ".");
	}
	routes::route_storage::instance().build(conf.api_root);

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
		fmtlog::setThreadName(("worker-" + std::to_string(worker_id)).c_str());

		data.loop->bind_to_thread();
		logi("Worker is ready to process requests");
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
		timespec sleep_time{.tv_nsec = (long) 1e8};  // 100 ms
		nanosleep(&sleep_time, nullptr);
		fmtlog::poll();
	}

	for (std::size_t id = 0; id < conf.request_workers; ++id) {
		workers[id].loop->stop_notify();
	}

	logi("Waiting for workers to process all remaining requests...");
	fmtlog::poll();

	for (std::size_t id = 0; id < conf.request_workers; ++id) {
		workers[id].t.join();
	}
	fmtlog::poll();
}