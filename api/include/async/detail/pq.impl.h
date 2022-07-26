/* ==== async::pq::connection ==== */
template <typename... Params>
inline coro<result> connection::exec(const char *command, Params &&...params) const {
	detail::params_lowerer<Params...> lowered{
		std::index_sequence_for<Params...>{},
		std::forward<Params>(params)...,
	};
	return detail::exec(*conn, command, sizeof...(Params), lowered.values, lowered.lengths,
						lowered.formats);
}

/* ==== async::pq::result ==== */
template <typename... Ts>
inline std::tuple<Ts...> result::expect1() const {
	if (rows() != 1) {
		throw db_error{"Expected 1 row as a result"};
	}
	return *(typed_result<Ts...>{res}.begin());
}

/* ==== async::pq::iterable_typed_result ==== */
template <typename... Ts>
template <std::size_t... Is>
std::tuple<Ts...> typed_result<Ts...>::iterator<Is...>::operator*() const {
	return {(PQgetisnull(res->res.get(), int(i), int(Is))
				 ? Ts{}
				 : detail::oid_decoder<Ts>::get(PQgetvalue(res->res.get(), int(i), int(Is)),
												PQgetlength(res->res.get(), int(i), int(Is)),
												res->oids[Is]))...};
}

template <typename... Ts>
typed_result<Ts...>::typed_result(raw_result res_) : res(res_) {
	if (PQnfields(res.get()) != SIZE) {
		throw db_error{"Unexpected number of columns"};
	}
	std::size_t i = 0;
	((oids[i] = PQftype(res.get(), int(i)), detail::check_type<Ts>(oids[i], i), ++i), ...);
}

/* ==== async::pq::detail::params_lowerer ==== */
template <typename... Params>
template <std::size_t idx, typename T>
inline void detail::params_lowerer<Params...>::apply(T &&param) {
	using U = std::decay_t<T>;
	formats[idx] = 0;

	if constexpr (std::same_as<U, std::string>) {
		values[idx] = param.data();
		lengths[idx] = int(param.size());
		formats[idx] = std::count(param.begin(), param.end(), 0) ? 1 : 0;
	} else if constexpr (std::same_as<U, std::string_view>) {
		buff[idx] = param;
		values[idx] = buff[idx].data();
		lengths[idx] = int(param.size());
		formats[idx] = std::count(param.begin(), param.end(), 0) ? 1 : 0;
	} else if constexpr (std::same_as<U, char *> || std::same_as<U, const char *>) {
		values[idx] = param;
	} else if constexpr (std::integral<U>) {
		buff[idx] = std::to_string(param);
		values[idx] = (*buff[idx]).data();
	} else {
		lower_param(std::forward<T>(param), values[idx], lengths[idx], formats[idx], buff[idx]);
	}
}
