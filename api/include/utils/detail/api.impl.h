template <typename T>
inline T expect(fcgx::request_t *r) {
	T result;
	if (!result.ParseFromString(r->raw_body)) {
		err(r, api::INVALID_QUERY);
	}
	return result;
}

template <typename T>
inline void ok(fcgx::request_t *r, const typename T::initializable_type &response) {
	std::string data;
	T obj{response};
	obj.SerializeToString(&data);
	send_raw(r, api::OK, data);
}

template <typename T>
inline void ok(fcgx::request_t *r, const T &response) {
	std::string data;
	response.SerializeToString(&data);
	send_raw(r, api::OK, data);
}
