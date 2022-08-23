#include "utils/api.h"
using namespace utils;

void utils::err(fcgx::request_t *r, api::ErrorCode code) {
	err_nothrow(r, code);
#if KEGE_LOG_DEBUG_ENABLED
	if (code == api::ACCESS_DENIED) {
		throw access_denied_error(r->request_uri);
	} else {
		throw expected_error(r->request_uri + ", code=" + std::to_string(code));
	}
#else
	throw expected_error();
#endif
}

void utils::err_nothrow(fcgx::request_t *r, api::ErrorCode code) {
	static const std::map<api::ErrorCode, int> HTTP_REMAP = {
		{api::INTERNAL_ERROR, 500}, {api::ACCESS_DENIED, 403},       {api::INVALID_SERVICE, 400},
		{api::INVALID_QUERY, 400},  {api::INVALID_CREDENTIALS, 403}, {api::EXTREMELY_SORRY, 400}};

	if (!r->is_meta_fixed) {
		auto it = HTTP_REMAP.find(code);
		if (it != HTTP_REMAP.end()) {
			r->headers.push_back("Status: " + std::to_string(it->second));
		}
	}
	send_raw(r, code, "");
}

void utils::send_raw(fcgx::request_t *r, api::ErrorCode code, std::string_view data) {
	api::Response response;
	response.set_code(code);
	response.set_response(std::string(data));
	response.SerializeToOstream(&r->out);
}
