/* ==== async::pq::connection ==== */
template <typename... Params>
inline coro<result> connection::exec(const char *command, Params &&...params) const {
	detail::params_lowerer<Params...> lowered{std::forward<Params>(params)...};
	return detail::exec(get_raw_connection(), command, sizeof...(Params), lowered.values,
						lowered.lengths, lowered.formats);
}

/* ==== async::pq::result ==== */
template <typename... T>
inline std::tuple<T...> result::expect1() const {
	if (rows() != 1) {
		throw db_error{"Expected 1 row as a result"};
	}
	return *(iterable_typed_result<T...>{res}.begin());
}

/* ==== async::pq::iterable_typed_result ==== */
template <typename... T>
template <std::size_t... Is>
template <typename U, std::size_t j>
inline U iterable_typed_result<T...>::iterator<Is...>::get_cell_value() {
	bool is_null = PQgetisnull(res->res.get(), int(i), int(j));
	if (is_null) {
		return {};
	}
	if constexpr (detail::is_optional(U{})) {
		return {detail::get_raw_cell_value<typename U::value_type>(res->res.get(), res->oids, int(i), int(j))};
	} else {
		return detail::get_raw_cell_value<U>(res->res.get(), res->oids, int(i), int(j));
	}
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

template <typename... Params>
template <std::size_t... Is>
inline detail::params_lowerer<Params...>::params_lowerer(std::index_sequence<Is...>,
														 Params &&...params) {
	(apply<Is>(std::forward<Params>(params)), ...);
}

template <typename... Params>
inline detail::params_lowerer<Params...>::params_lowerer(Params &&...params)
	: params_lowerer(std::index_sequence_for<Params...>{}, std::forward<Params>(params)...) {}

template<typename T>
inline T detail::get_raw_cell_value(PGresult *res, unsigned *oids, int i, int j) {
	if constexpr (std::same_as<T, std::string>) {
		return std::string(get_raw_cell_value<std::string_view>(res, oids, i, j));
	}

	using func = T(*)(char const *, int);

	unsigned oid = oids[j];
	std::pair key{oid, (void *) nullptr};
	auto it = std::ranges::lower_bound(detail::pq_decoder::map, key);
	if (it == detail::pq_decoder::map.end() || it->first != oid) {
		throw pq::db_error("unknown oid " + std::to_string(oid) + " at column " + std::to_string(j));
	}

	return ((func) it->second)(PQgetvalue(res, i, j), PQgetlength(res, i, j));
}
