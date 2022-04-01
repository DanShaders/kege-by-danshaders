#include "logging.h"

std::mutex log_mutex;
FILE *INFO_FILE, *ERROR_FILE, *ADMIN_FILE;
thread_local std::string logging_thread_name;

void logging::init() {
	INFO_FILE = stdout;
	ERROR_FILE = stdout;
	ADMIN_FILE = stdout;
}

void logging::set_thread_name(const std::string_view &name) {
	logging_thread_name = name;
}

void _log(const char *prefix, const char *message, std::size_t message_len, FILE *file) {
	const int MAX_BUFFER = 25;
	char buffer[MAX_BUFFER];

	if (logging_thread_name.empty()) {
		std::ostringstream out;
		out << "ID~" << std::this_thread::get_id();
		logging_thread_name = out.str();
	}

	{
		std::lock_guard<std::mutex> guard(log_mutex);

		timeval tv;
		gettimeofday(&tv, nullptr);
		int millis = int(tv.tv_usec / 1000);
		if (millis >= 1000) {
			millis -= 1000;
			tv.tv_sec++;
		}

		strftime(buffer, MAX_BUFFER - 1, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
		fprintf(file, "[%s.%03d] [%s] [%s] %.*s\n", buffer, millis, logging_thread_name.c_str(),
				prefix, int(message_len), message);
		fflush(file);
	}
}

void logging::info(const std::string_view &message) {
	_log("info", message.data(), message.size(), INFO_FILE);
}

void logging::warn(const std::string_view &message) {
	_log("warn", message.data(), message.size(), INFO_FILE);
}

void logging::error(const std::string_view &message) {
	_log("error", message.data(), message.size(), ERROR_FILE);
}

void logging::admin(const std::string_view &message) {
	_log("admin", message.data(), message.size(), ADMIN_FILE);
}

#ifdef KEGE_LOG_DEBUG_ENABLED
void logging::debug(const std::string_view &message) {
	_log("debug", message.data(), message.size(), INFO_FILE);
}
#endif