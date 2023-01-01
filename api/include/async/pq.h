#pragma once

#include <libpq-fe.h>

#include "async/coro.h"
#include "async/libev-event-loop.h"

namespace async::pq {
using raw_result = std::shared_ptr<PGresult>;

struct connection_storage;
class connection;
class connection_pool;
template <typename... Ts>
class typed_result;
class result;

using timestamp = std::chrono::system_clock::time_point;

class db_error : public std::runtime_error {
  using runtime_error::runtime_error;
};

template <typename... Ts>
struct type_sequence {};

template <typename Ts, typename Params>
struct prepared_sql_query {
  char const* sql;
};

class result {
private:
  raw_result res;

public:
  result(PGresult* raw);

  size_t rows() const;
  size_t columns() const;

  template <typename... Ts>
  typed_result<Ts...> as() const {
    return {res};
  }

  /** @deprecated */
  template <typename... Ts>
  auto iter() const {
    return as<Ts...>();
  }

  void expect0() const;

  template <typename... Ts>
  std::tuple<Ts...> expect1() const;
};

template <typename... Ts>
class typed_result {
private:
  const static size_t SIZE = sizeof...(Ts);

  raw_result res;
  std::array<int, SIZE> oids;

public:
  template <size_t... Is>
  class iterator {
  private:
    size_t i;
    typed_result<Ts...> const* res;

  public:
    iterator(size_t i_, typed_result<Ts...> const* res_, std::index_sequence<Is...>)
        : i(i_), res(res_) {}

    std::tuple<Ts...> operator*() const;

    iterator& operator++() {
      ++i;
      return *this;
    }

    std::strong_ordering operator<=>(iterator const&) const = default;
  };

  typed_result(raw_result res_);

  size_t rows() const {
    return std::size_t(PQntuples(res.get()));
  }

  size_t columns() const {
    return std::size_t(PQnfields(res.get()));
  }

  void expect0() const {
    if (rows()) {
      throw db_error{"Expected 0 rows as a result"};
    }
  }

  std::tuple<Ts...> expect1() const {
    if (rows() != 1) {
      throw db_error{"Expected 1 row as a result"};
    }
    return *begin();
  }

  auto begin() const {
    return iterator{(size_t) 0, this, std::index_sequence_for<Ts...>{}};
  }

  auto end() const {
    return iterator{(size_t) PQntuples(res.get()), this, std::index_sequence_for<Ts...>{}};
  }
};

struct connection_storage {
  FIXED_CLASS(connection_storage)

  PGconn* conn;
  socket_storage sock;

  connection_storage() {}

  ~connection_storage();
};

using raw_connection = std::shared_ptr<connection_storage>;

class connection {
  ONLY_DEFAULT_MOVABLE_CLASS(connection)

private:
  raw_connection conn;
  connection_pool* pool;
  bool has_active_transaction = false;

  static coro<void> rollback_and_return(raw_connection conn, connection_pool* pool);

public:
  connection(raw_connection conn_, connection_pool* pool_);
  ~connection();

  PGconn* get_raw_connection() const noexcept;

  coro<void> transaction();
  coro<void> commit();
  coro<void> rollback();

  template <typename... Params>
  coro<result> exec(char const* command, Params&&... params) const;

  template <typename... Params, typename... Ts>
  coro<typed_result<Ts...>> exec(
      prepared_sql_query<type_sequence<std::decay_t<Params>...>, type_sequence<Ts...>> command,
      Params&&... params) const {
    result res = co_await exec(command.sql, std::forward<Params>(params)...);
    co_return res.as<Ts...>();
  }
};

class connection_pool : public event_source {
  ONLY_DEFAULT_MOVABLE_CLASS(connection_pool)

private:
  friend connection;

  struct impl;
  std::unique_ptr<impl> pimpl;

  void return_connection(raw_connection conn) noexcept;

public:
  static thread_local std::shared_ptr<connection_pool> local;

  using clock = std::chrono::steady_clock;

  struct creation_info {
    std::string_view db_path;
    size_t connections = 1;
    clock::duration creation_cooldown;
  };

  connection_pool(creation_info const& info);
  ~connection_pool();

  void bind_to_thread() override;
  void on_init() override;
  void on_stop() override;

  coro<connection> get_connection();
};

namespace detail {
  /** @private */
  template <typename T>
  struct pq_binary_converter {
    static bool is_valid(int oid);
    static T get(char* data, int length, int oid);
    static std::optional<std::string_view> set(T const& obj, std::string& buff);
  };

  template <typename T>
  struct pq_binary_converter<std::optional<T>> {
    static bool is_valid(int oid) {
      return pq_binary_converter<T>::is_valid(oid);
    }

    static T get(char* data, int length, int oid) {
      return pq_binary_converter<T>::get(data, length, oid);
    }
  };

  template <std::ranges::sized_range T>
    requires(!std::same_as<T, std::string_view> && !std::same_as<T, std::string>)
  struct pq_binary_converter<T> {
    static std::optional<std::string_view> set(T const& obj, std::string& buff) {
      assert(obj.size() <= std::numeric_limits<int>::max());

      pq_binary_converter<int>::set(1, buff);  // number of dimensions
      pq_binary_converter<int>::set(0, buff);  // unused
      pq_binary_converter<Oid>::set(std::numeric_limits<Oid>::max(),
                                    buff);                                // force to infer Oid
      pq_binary_converter<int>::set(static_cast<int>(obj.size()), buff);  // size
      pq_binary_converter<int>::set(1, buff);                             // starting index

      for (auto&& elem : obj) {
        buff.append(4, (char) 0);

        size_t pos = buff.size();
        auto result = pq_binary_converter<std::decay_t<decltype(elem)>>::set(elem, buff);
        if (result) {
          buff.append(result->begin(), result->end());
        }
        size_t sz = buff.size() - pos;
        assert(sz <= std::numeric_limits<int>::max());

        uint32_t to_write = htobe32(static_cast<uint32_t>(sz));
        std::copy_n(reinterpret_cast<char*>(&to_write), 4, buff.begin() + pos - 4);
      }
      return {};
    }
  };

  /** @private */
  template <typename T>
  void check_type(int oid, size_t i) {
    if (!pq_binary_converter<T>::is_valid(oid)) {
      throw db_error{
          fmt::format("Oid {} at column {} cannot be converted to {}", oid, i, typeid(T).name())};
    }
  }

  /** @private */
  template <size_t SIZE>
  struct params_lowerer {
    char const* values[SIZE];
    size_t start_idx[SIZE];
    int lengths[SIZE], formats[SIZE];
    std::string buff;

    template <typename T>
    void apply(size_t i, T&& param) {
      start_idx[i] = buff.size();
      size_t len;
      auto result = pq_binary_converter<std::decay_t<T>>::set(param, buff);
      if (result) {
        values[i] = result->data();
        len = result->size();
        start_idx[i] = std::string::npos;
      } else {
        len = buff.size() - start_idx[i];
      }
      assert(len <= std::numeric_limits<int>::max());
      lengths[i] = static_cast<int>(len);
    }

    template <size_t... Is, typename... Params>
    params_lowerer(std::index_sequence<Is...>, Params&&... params) {
      static_assert(sizeof...(Is) == sizeof...(Params) && sizeof...(Is) == SIZE);

      (apply(Is, std::forward<Params>(params)), ...);
      for (size_t i = 0; i < SIZE; ++i) {
        formats[i] = 1;
        if (start_idx[i] != std::string::npos) {
          values[i] = buff.data() + start_idx[i];
        }
      }
    }
  };

  /** @private */
  coro<result> exec(connection_storage& conn, char const* command, int size, char const* values[],
                    int lengths[], int formats[]);
}  // namespace detail

#include "detail/pq.impl.h"
}  // namespace async::pq