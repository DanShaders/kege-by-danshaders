#pragma once

#include "stdafx.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
extern "C" {
#include <lexbor/html/html.h>
}
#pragma GCC diagnostic pop

namespace utils {
using transform_handle_t = std::function<lxb_dom_node_t*(lxb_dom_node_t*, void*)>;

struct transform_rule {
  std::string selector;
  transform_handle_t handle;
};

struct transform_rules_t;
using transform_rules = std::shared_ptr<transform_rules_t>;

transform_rules compile_transform_rules(std::vector<transform_rule> const& rules);

class html_fragment {
  FIXED_CLASS(html_fragment)

private:
  lxb_html_document_t* root;

public:
  html_fragment(std::string_view html);
  ~html_fragment();

  void transform(transform_rules rules, void* ctx);
  std::string get_html();
};
}  // namespace utils
