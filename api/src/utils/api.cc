#include "utils/api.h"
using namespace utils;

void utils::err(fcgx::request_t *r, api::ErrorCode code) {
	err_nothrow(r, code);
	throw expected_error();
}

void utils::err_nothrow(fcgx::request_t *r, api::ErrorCode code) {
	send_raw(r, code, "");
}

void utils::send_raw(fcgx::request_t *r, api::ErrorCode code, const std::string &data) {
	api::Response response;
	response.set_code(code);
	response.set_response(data);
	response.SerializeToOstream(&r->out);
}
