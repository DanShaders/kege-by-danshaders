/* ==== async::pq::connection ==== */
template <typename... Params>
inline coro<result> connection::exec(const char *command, Params &&...params) {
	detail::params_lowerer<Params...> lowered{std::forward<Params>(params)...};
	return detail::exec(get_raw_connection(),
		command, sizeof...(Params), lowered.values, lowered.lengths, lowered.formats);
}

/* ==== async::pq::result::iter ==== */
template <typename... T>
inline iterable_typed_result<T...> result::iter() const {
	return {this};
}

/* ==== async::pq::detail::params_lowerer ==== */
template <typename... Params>
template <std::size_t idx, typename T>
inline void detail::params_lowerer<Params...>::apply(T &&param) {
	using U = std::decay_t<T>;
	formats[idx] = 0;

	if constexpr (std::same_as<U, std::string>) {
		values[idx] = param.data();
		lengths[idx] = param.size();
		formats[idx] = std::count(param.begin(), param.end(), 0) ? 1 : 0;
	} else if constexpr (std::same_as<U, std::string_view>) {
		buff[idx] = param;
		values[idx] = buff[idx].data();
		lengths[idx] = param.size();
		formats[idx] = std::count(param.begin(), param.end(), 0) ? 1 : 0;
	} else if constexpr (std::same_as<U, char *> || std::same_as<U, const char *>) {
		values[idx] = param;
	} else if constexpr (std::integral<U>) {
		buff[idx] = std::to_string(param);
		values[idx] = (*buff[idx]).data();
	} else {
		lower_param(std::forward<T>(param), values[idx], lengths[idx], formats[idx],
					buff[idx]);
	}
}

template <typename... Params>
template <std::size_t... Is>
inline detail::params_lowerer<Params...>::params_lowerer(std::index_sequence<Is...>, Params &&...params) {
	(apply<Is>(std::forward<Params>(params)), ...);
}

template <typename... Params>
inline detail::params_lowerer<Params...>::params_lowerer(Params &&...params)
	: params_lowerer(std::index_sequence_for<Params...>{}, std::forward<Params>(params)...) {}
