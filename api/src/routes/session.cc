#include "routes/session.h"

#include "async/libev-event-loop.h"
#include "utils/api.h"
#include "utils/common.h"
#include "utils/crypto.h"
using namespace routes;

namespace {
// We do not want to leak information about the session storage via session_id comparison times,
// so we always sign user-supplied session_id with the following sign_key and then compare only
// hashed values.
utils::signature_t get_signed_id(std::string_view id) {
  static std::string sign_key = utils::urandom(32);
  return utils::hmac_sign(id, sign_key);
}

using session_clock = std::chrono::system_clock;

struct trie_node {
  std::array<std::shared_ptr<trie_node>, 16> children;
  std::shared_ptr<session> ptr;

  std::shared_ptr<trie_node> clone() const {
    return std::make_shared<trie_node>(children, ptr);
  }

  std::shared_ptr<trie_node> gc_dfs(size_t& nodes_count) const {
    if (ptr) {
      bool should_gc = false;
      should_gc |= ptr->logged_out;
      auto last_activity = session_clock::time_point(session_clock::duration(ptr->last_activity));
      should_gc |= (session_clock::now() - last_activity) > session_logout_afk_time;

      if (should_gc) {
        return nullptr;
      } else {
        ++nodes_count;
        return std::make_shared<trie_node>(decltype(children){}, ptr);
      }
    } else {
      std::shared_ptr<trie_node> new_node;
      for (size_t i = 0; i < 16; ++i) {
        auto child = children[i];
        if (!children[i]) {
          continue;
        }

        auto new_child = child->gc_dfs(nodes_count);
        if (!new_child) {
          continue;
        }

        if (!new_node) {
          ++nodes_count;
          new_node = std::make_shared<trie_node>();
        }
        new_node->children[i] = new_child;
      }
      return new_node;
    }
  }
};

// FIXME: Do not block worker thread entirely, just one coroutine.
std::mutex write_lock;
// FIXME: Do not use atomic<shared_ptr>
std::atomic<std::shared_ptr<trie_node>> root_node;

coro<void> run_session_gc() {
  logi("Running session GC (note: locking all login attemps)...");

  size_t nodes_count = 0;
  {
    // FIXME: This is a "stop the world" GC, which I'm not really a fan of.
    std::lock_guard write_guard(write_lock);
    auto root = root_node.load();
    if (root) {
      root_node = root->gc_dfs(nodes_count);
    }
  }
  logi("Session GC done, {} nodes in session trie", nodes_count);

  auto continuation = [] { schedule_detached(run_session_gc()); };
  async::set_timeout(std::make_unique<std::function<void()>>(continuation), std::chrono::hours(1));
  // Прыщи? Ноль! Угри? Ноль! Boilerplate? Ноль!

  co_return;
}
}  // namespace

bool session::is_owner_of(int64_t id) const {
  auto current_range = id_ranges.load();
  while (current_range) {
    if (current_range->start <= id && id < current_range->ptr && id < current_range->end) {
      return true;
    }
    current_range = current_range->next.load();
  }
  return false;
}

api::UserInfo::initializable_type session::serialize() const {
  return {.user_id = user_id, .username = username, .display_name = display_name, .perms = perms};
}

coro<void> session::save(std::shared_ptr<session> s, std::string_view session_id) {
  std::lock_guard write_guard(write_lock);
  auto signed_id = get_signed_id(session_id);

  auto new_root = root_node.load();
  if (!new_root) {
    new_root = std::make_shared<trie_node>();
  } else {
    new_root = new_root->clone();
  }
  auto current = new_root.get();

  for (size_t i = 0; i < 2 * utils::signature_length; ++i) {
    unsigned nibble = signed_id[i / 2] >> (i % 2 * 4) & 15;
    auto& child = current->children[nibble];

    if (!child) {
      child = std::make_shared<trie_node>();
      current = child.get();
    } else {
      child = child->clone();
      current = child.get();
    }
  }

  if (current->ptr) {
    throw std::runtime_error("Session was already saved in the storage");
  }
  s->last_activity = session_clock::now().time_since_epoch().count();
  current->ptr = s;

  root_node = new_root;

  static bool gc_summoned = false;
  if (!gc_summoned) [[unlikely]] {
    gc_summoned = true;

    // FIXME: Run GC in a separate thread
    auto continuation = [] { schedule_detached(run_session_gc()); };
    async::set_timeout(std::make_unique<std::function<void()>>(continuation),
                       std::chrono::hours(1));
  }
  co_return;
}

coro<std::shared_ptr<session>> routes::require_auth(fcgx::request_t* r, Permission mask) {
  auto raise_access_denied = [&r] { utils::err(r, api::ACCESS_DENIED); };

  auto cookies_it = r->cookies.find("kege-session");
  if (cookies_it == r->cookies.end()) {
    raise_access_denied();
  }
  auto signed_id = get_signed_id(utils::b16_decode(cookies_it->second));

  auto root = root_node.load();
  auto current = root.get();

  if (!current) {
    raise_access_denied();
  }

  for (size_t i = 0; i < 2 * utils::signature_length; ++i) {
    unsigned nibble = signed_id[i / 2] >> (i % 2 * 4) & 15;
    auto& child = current->children[nibble];

    if (!child) {
      raise_access_denied();
    } else {
      current = child.get();
    }
  }

  auto s = current->ptr;
  auto i_mask = static_cast<unsigned>(mask);
  if (!s || s->logged_out || (s->perms & i_mask) != i_mask) {
    raise_access_denied();
  }
  s->last_activity = session_clock::now().time_since_epoch().count();
  co_return s;
}
