#include "fcgx.h"
using namespace fcgx;

#include <ev.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "async/libev-event-loop.h"
#include "async/socket.h"
#include "logging.h"
#include "utils/common.h"

using async::coro, async::socket_storage, async::detail::ev_with_arg;

typedef struct ev_loop ev_loop_t;

/* ==== fcgx::fcgx_streambuf ==== */
fcgx_streambuf::fcgx_streambuf(FCGX_Stream *fcgx_) : fcgx(fcgx_) {}

std::streamsize fcgx_streambuf::xsputn(const char_type *s, std::streamsize n) {
	return (std::streamsize) FCGX_PutStr((char *) s, (int) n, fcgx);
}

/* ==== fcgx::request_streambuf ==== */
std::streamsize request_streambuf::xsputn(const char_type *s, std::streamsize n) {
	if (request->is_meta_fixed) {
		if (!is_meta_fixed) {
			for (const auto &header : request->headers) {
				fcgx->sputn(header.c_str(), header.size());
				fcgx->sputn("\r\n", 2);
			}
			fcgx->sputn("content-type: ", 14);
			fcgx->sputn(request->mime_type.c_str(), request->mime_type.size());
			fcgx->sputn("\r\n\r\n", 4);

			auto view = pre_buffer->view();
			fcgx->sputn(view.data(), view.size());
			delete pre_buffer;
			pre_buffer = 0;
			is_meta_fixed = true;
		}
		fcgx->sputn(s, n);
	} else {
		pre_buffer->sputn(s, n);
	}
	return n;
}

request_streambuf::request_streambuf(std::streambuf *fcgx_)
	: fcgx(fcgx_), pre_buffer(new std::stringbuf()) {}

request_streambuf::~request_streambuf() {
	if (pre_buffer)
		delete pre_buffer;
	delete fcgx;
}

void request_streambuf::set_request(request_t *request_) {
	request = request_;
}

/* ==== fcgx::request_t ==== */
void request_t::finish() {
	if (!is_meta_fixed)
		fix_meta();
	out.rdbuf()->sputn(0, 0);
	FCGX_FFlush(raw->out);
#if defined(KEGE_FCGI_USE_ASYNC) && KEGE_FCGI_USE_ASYNC
	async::libev_event_loop::local->fcgx_schedule_close(raw);
#else
	FCGX_Finish_r(raw);
	delete raw;
#endif
	delete _rsb;
	delete this;
}

void request_t::fix_meta() {
	is_meta_fixed = true;
}

namespace {
bool is_rfc2045_tspecial(char c) {
	const static char SPECIALS[] = {'(',  ')', '<', '>', '@', ',', ';', ':',
									'\\', '"', '/', '[', ']', '?', '.', '='};
	return c <= 32 || std::count(SPECIALS, std::end(SPECIALS), c);
}

bool is_mime_type(const std::string_view &mime, const std::string_view &to_test) {
	return mime.starts_with(to_test) &&
		   (mime.size() <= to_test.size() || is_rfc2045_tspecial(mime[to_test.size()]));
}

#if defined(KEGE_FCGI_USE_ASYNC) && KEGE_FCGI_USE_ASYNC
bool fcgx_setnonblocking(FCGX_Request *req, bool nonblocking = true) {
	int fd = req->ipcFd;

	int flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return false;

	if (nonblocking)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) == -1)
		return false;
	return true;
}

bool fcgx_setstorage(socket_storage *&storage, FCGX_Request *req, bool nonblocking = true) {
	if (!nonblocking && !storage)
		return true;
	if (nonblocking) {
		storage = new socket_storage{};
		storage->fd = req->ipcFd;
		storage->flags = EV_READ;
		storage->loop = &*async::event_loop::local;
		async::libev_event_loop::local->socket_add(storage);
	} else {
		async::libev_event_loop::local->socket_del(storage);
	}
	return true;
}

int fcgx_try_async_read(FCGX_Stream *stream, char *str, int n, int &already) {
	if (already == n || stream->isClosed)
		return already;

	int ret = FCGX_GetStr(str + already, n - already, stream);
	already += ret;
	if (stream->isClosed && stream->FCGI_errno &&
		(stream->FCGI_errno == EAGAIN || stream->FCGI_errno == EWOULDBLOCK)) {
		stream->FCGI_errno = 0;
		stream->isClosed = false;
		errno = EAGAIN;
		return -1;
	}
	return already;
}

coro<int> fcgx_async_read_str(char *str, int n, FCGX_Stream *stream, socket_storage *&storage,
							  int &tmp, FCGX_Request *req) {
	if (storage)
		storage->initial_lock.store(true);
	int ret = FCGX_GetStr(str, n, stream);
	if (!(stream->isClosed && stream->FCGI_errno &&
		  (stream->FCGI_errno == EAGAIN || stream->FCGI_errno == EWOULDBLOCK))) {
		co_return ret;
	}
	stream->isClosed = false;
	stream->FCGI_errno = 0;
	tmp = ret;
	if (!storage)
		fcgx_setstorage(storage, req, true);
	co_return co_await async::eagain_performer{
		storage, 0, fcgx_try_async_read, std::move(stream), std::move(str), std::move(n), tmp};
}
#else
bool fcgx_setnonblocking(FCGX_Request *, bool = true) {
	return true;
}

bool fcgx_setstorage(socket_storage *&, FCGX_Request *, bool = true) {
	return true;
}

coro<int> fcgx_async_read_str(char *str, int n, FCGX_Stream *stream, socket_storage *&, int &,
							  FCGX_Request *) {
	co_return FCGX_GetStr(str, n, stream);
}
#endif

void ensure(bool cond, const std::string &msg, int err = errno) {
	if (cond) {
		throw std::system_error(err, std::system_category(), msg);
	}
}

class scope_guard {
private:
	std::function<void()> runnable;

public:
	template <typename T>
	scope_guard(const T &func) : runnable(func) {}

	~scope_guard() {
		runnable();
	}
};
}  // namespace

/* ==== fcgx::server::impl ==== */
struct server::impl {
	int fd;
	std::function<void(FCGX_Request *)> handler;
	ev_loop_t *loop;
	ev_with_arg<ev_io> accept_ev;
	FCGX_Request *req;

	impl(const config::hl_socket_address &addr, int queue_size,
		 const std::function<void(FCGX_Request *)> &handler_)
		: handler(handler_) {
		req = new FCGX_Request;
		if (addr.use_unix_sockets) {
			fd = FCGX_OpenSocket(addr.path.c_str(), queue_size);
			ensure(fd < 0, "socket creation failed");
			if (addr.perms != -1) {
				ensure(chmod(addr.path.c_str(), addr.perms),
					   "changing socket file permissions failed");
			}
			ensure(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1,
				   "making socket non blocking failed");
		} else {
			std::string port_str = std::to_string(addr.port);
			std::string sock_id = addr.path + ":" + port_str;
			addrinfo *result;
			ensure(getaddrinfo(addr.path.c_str(), port_str.c_str(), 0, &result),
				   "getaddrinfo for " + sock_id + " failed");
			scope_guard result_guard([&] { freeaddrinfo(result); });

			for (addrinfo *it = result; it; it = it->ai_next) {
				fd = socket(it->ai_family, it->ai_socktype | SOCK_NONBLOCK, it->ai_protocol);
				if (fd < 0) {
					continue;
				}
				int optval = 1;
				ensure(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)),
					   "setting SO_REUSEPORT on FCGX socket failed");
				ensure(bind(fd, it->ai_addr, it->ai_addrlen), "binding socket failed");
				break;
			}
			ensure(fd < 0, "out of options to bind socket to " + sock_id);
			ensure(listen(fd, queue_size), "listen failed");
		}
	}

	~impl() {
		ev_io_stop(loop, &accept_ev.w);
		delete req;
		close(fd);
	}

	static void accept_cb(ev_loop_t *, ev_io *w, int) {
		auto p = (impl *) ((ev_with_arg<ev_io> *) w)->arg;

		while (true) {
			FCGX_InitRequest(p->req, p->fd, FCGI_FAIL_ACCEPT_ON_INTR);
			if (FCGX_Accept_r(p->req)) {
				break;
			}
			p->handler(p->req);
			p->req = new FCGX_Request;
		}
	}

	void on_init() {
		loop = (ev_loop_t *) async::libev_event_loop::get()->get_underlying_loop();

		accept_ev.arg = this;
		ev_io_init(&accept_ev.w, accept_cb, fd, EV_READ);
		ev_io_start(loop, &accept_ev.w);
	}
};

/* ==== fcgx::server ==== */
server::server(const config::hl_socket_address &addr, int queue_size,
			   const std::function<void(FCGX_Request *)> &handler) {
	pimpl = new impl{addr, queue_size, handler};
}

void server::on_init() {
	pimpl->on_init();
}

void server::on_stop() {
	delete pimpl;
}

coro<request_t *> fcgx::from_raw(FCGX_Request *raw) {
	char *remote_ip = FCGX_GetParam("REMOTE_ADDR", raw->envp),
		 *method = FCGX_GetParam("REQUEST_METHOD", raw->envp),
		 *query_string = FCGX_GetParam("QUERY_STRING", raw->envp),
		 *request_uri = FCGX_GetParam("DOCUMENT_URI", raw->envp);

	if (!remote_ip || !method || !query_string || !request_uri) {
		FCGX_PutS("status: 400\r\n\r\n", raw->out);
		FCGX_Finish_r(raw);
		delete raw;
		co_return 0;
	}

	const int FCGX_CHUNK_SIZE = 4096;
	char chunk[FCGX_CHUNK_SIZE];
	std::size_t clen;

	body_type_t body_type = BODY_UNINITIALIZED;
	json body;
	std::string data, raw_body;

	socket_storage *storage = 0;
	int tmp;

	char *mime_type = FCGX_GetParam("HTTP_CONTENT_TYPE", raw->envp);

	fcgx_setnonblocking(raw);
#define READ_CHUNK \
	(clen = co_await fcgx_async_read_str(chunk, FCGX_CHUNK_SIZE, raw->in, storage, tmp, raw))

	try {
		if (mime_type && is_mime_type(mime_type, "application/x-www-form-urlencoded")) {
			/* Body is url-encoded string `<p1>=<v1>&<p2>=<v2>&...`*/
			body = json::object();
			bool on_key = true;
			std::string key, value;
			while (READ_CHUNK) {
				for (char c : std::string_view(chunk, chunk + clen)) {
					if (c == '&') {
						body[utils::url_decode(key)] = utils::url_decode(value);
						key = "";
						value = "";
					} else if (on_key && c == '=') {
						on_key = false;
					} else {
						(on_key ? key : value) += c;
					}
				}
			}
			if (key.size() || value.size())
				body[utils::url_decode(key)] = utils::url_decode(value);
			body_type = BODY_FORM_URL;

		} else if (mime_type && is_mime_type(mime_type, "application/json")) {
			/* Body is a JSON object */
			while (READ_CHUNK)
				data += std::string_view(chunk, chunk + clen);

			try {
				body = json::parse(data);
				body_type = BODY_JSON;
			} catch (const std::exception &e) {
				body["_err"] = e.what();
				raw_body = data;
				body_type = BODY_DECODE_ERROR;
			} catch (...) {
				body["_err"] = "unknown error";
				raw_body = data;
				body_type = BODY_DECODE_ERROR;
			}
		}

		if (body_type == BODY_UNINITIALIZED) {
			/* Body is plaintext */
			while (READ_CHUNK)
				data += std::string_view(chunk, chunk + clen);
			body.clear();
			raw_body = data;
			body_type = BODY_PLAIN_TEXT;
		}
	} catch (...) {
		body.clear();
		raw_body = "";
	}

#undef READ_CHUNK

	fcgx_setnonblocking(raw, false);
	fcgx_setstorage(storage, raw, false);

	fcgx_streambuf *fbuf = new fcgx_streambuf(raw->out);
	request_streambuf *rbuf = new request_streambuf(fbuf);

	// clang-format off
	request_t* r = new request_t{
		raw,
		rbuf,
		std::string(remote_ip),
		std::string(method),
		std::string(request_uri),
		std::string(query_string),
		utils::parse_query_string(query_string),
		body_type,
		std::move(body),
		std::move(raw_body),

		{},
		"application/json;charset=utf-8",
		std::ostream(rbuf)
	};
	// clang-format on

	rbuf->set_request(r);

	co_return r;
}
