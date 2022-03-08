#pragma once

#include "stdafx.h"

#include <curl/curl.h>

#include "event-loop.h"

namespace async {
struct curl_storage;
class curl;
class curl_result;
class curl_performer;
class curl_event_source;

class curl {
	ONLY_DEFAULT_MOVABLE_CLASS(curl)

private:
	std::shared_ptr<curl_storage> storage;

public:
	curl();

	void reset();

	template <typename T>
	CURLcode set_opt(CURLoption, T);
	CURLcode set_url(const std::string &url);
	CURL *get_handle() const;

	void write_body_to(std::streambuf *buf);

	curl_performer get_headers();
	curl_performer perform();
};

class curl_result {
	ONLY_DEFAULT_MOVABLE_CLASS(curl_result)

private:
	friend curl_performer;

	std::shared_ptr<curl_storage> storage;

	curl_result(std::shared_ptr<curl_storage> storage_);

public:
	int status() const;
	const std::vector<std::string> &raw_headers() const;
	const std::string &body() const;
	const std::string &mime_type() const;
};

class curl_performer {
	ONLY_DEFAULT_MOVABLE_CLASS(curl_performer)

private:
	std::shared_ptr<curl_storage> storage;

public:
	curl_performer(std::shared_ptr<curl_storage> storage_);

	bool await_ready();
	curl_result await_resume();
	template <typename T>
	void await_suspend(std::coroutine_handle<T> &) noexcept;
	void await_suspend(event_loop_work &&work) noexcept;
};

class curl_event_source : public event_source {
private:
	struct impl;
	impl *pimpl;

public:
	static thread_local std::shared_ptr<curl_event_source> local;

	void bind_to_thread() override;
	void on_init() override;
	void on_stop() override;

	void schedule_perform(std::shared_ptr<curl_storage> storage);
};

#include "detail/curl.impl.h"
}  // namespace async