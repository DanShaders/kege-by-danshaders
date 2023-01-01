/* ==== async::pq::connection ==== */
template <typename... Params>
coro<result> connection::exec(char const* command, Params&&... params) const {
  detail::params_lowerer<sizeof...(Params)> lowered{
      std::index_sequence_for<Params...>{},
      std::forward<Params>(params)...,
  };
  co_return co_await detail::exec(*conn, command, sizeof...(Params), lowered.values,
                                  lowered.lengths, lowered.formats);
}

/* ==== async::pq::result ==== */
template <typename... Ts>
std::tuple<Ts...> result::expect1() const {
  if (rows() != 1) {
    throw db_error{"Expected 1 row as a result"};
  }
  return *(typed_result<Ts...>{res}.begin());
}

/* ==== async::pq::iterable_typed_result ==== */
template <typename... Ts>
template <size_t... Is>
std::tuple<Ts...> typed_result<Ts...>::iterator<Is...>::operator*() const {
  return {(PQgetisnull(res->res.get(), int(i), int(Is))
               ? Ts{}
               : detail::pq_binary_converter<Ts>::get(PQgetvalue(res->res.get(), int(i), int(Is)),
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
