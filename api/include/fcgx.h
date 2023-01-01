#pragma once

#include "stdafx.h"
#include "config.h"

#include <fcgiapp.h>

#include "async/coro.h"
#include "async/event-loop.h"
using async::coro;

namespace fcgx {
class fcgx_streambuf;
class request_streambuf;
class request_t;

class fcgx_streambuf : public std::streambuf {
private:
  FCGX_Stream* fcgx;

protected:
  virtual std::streamsize xsputn(char_type const*, std::streamsize);

public:
  fcgx_streambuf(FCGX_Stream*);
};

class request_streambuf : public std::streambuf {
private:
  std::streambuf* fcgx;
  std::stringbuf* pre_buffer;
  request_t* request;
  bool is_meta_fixed = false;

protected:
  virtual std::streamsize xsputn(char_type const*, std::streamsize);

public:
  request_streambuf(std::streambuf*);
  request_streambuf(request_streambuf&) = delete;
  request_streambuf(request_streambuf&&) = delete;
  ~request_streambuf();

  void set_request(request_t*);
};

enum body_type_t {
  BODY_UNINITIALIZED = 0,
  BODY_DECODE_ERROR,

  BODY_PLAIN_TEXT,
  BODY_JSON,
  BODY_FORM_URL
};

class request_t {
private:
  friend request_streambuf;

public:
  FCGX_Request* raw;
  request_streambuf* _rsb;

  std::string remote_ip, method, request_uri, raw_params;
  std::map<std::string, std::string, std::less<>> params, cookies;

  body_type_t body_type = BODY_UNINITIALIZED;
  json body;
  std::string raw_body;

  std::vector<std::string> headers;
  std::string mime_type;
  std::ostream out;
  bool is_meta_fixed = false;

  void fix_meta();
  void finish();
};

struct server : public async::event_source {
private:
  struct impl;
  impl* pimpl;

public:
  server(config::hl_socket_address const& addr, int queue_size,
         std::function<void(FCGX_Request*)> const& handler);

  void on_init() override;
  void on_stop() override;
};

coro<request_t*> from_raw(FCGX_Request* raw);
}  // namespace fcgx