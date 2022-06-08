#include "utils/html.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
extern "C" {
#define LXB_CSS_STYLESHEET_H
#include <lexbor/css/css.h>
#include <lexbor/selectors/selectors.h>
}
#pragma GCC diagnostic pop

#include "utils/api.h"
using namespace utils;

/* ==== utils::transform_rules_t ==== */
struct utils::transform_rules_t {
	struct rule {
		lxb_css_selector_list_t *selector;
		transform_handle_t handle;
	};
	std::vector<rule> rules;
};

namespace {
thread_local std::shared_ptr<lxb_html_parser_t> html_parser = nullptr;
thread_local std::shared_ptr<lxb_css_parser_t> css_parser = nullptr;
thread_local std::shared_ptr<lxb_css_selectors_t> css_selectors = nullptr;
thread_local std::shared_ptr<lxb_selectors_t> selectors = nullptr;

void ensure_parsers() {
	if (!html_parser) {
		auto local = lxb_html_parser_create();
		if (lxb_html_parser_init(local) != LXB_STATUS_OK) {
			lxb_html_parser_destroy(local);
			goto ensure_failed;
		}
		html_parser = std::shared_ptr<lxb_html_parser_t>(
			local, [](lxb_html_parser_t *ptr) { lxb_html_parser_destroy(ptr); });
	}

	if (!css_parser) {
		auto local = lxb_css_parser_create();
		if (lxb_css_parser_init(local, nullptr, nullptr) != LXB_STATUS_OK) {
			lxb_css_parser_destroy(local, true);
			goto ensure_failed;
		}
		css_parser = std::shared_ptr<lxb_css_parser_t>(
			local, [](lxb_css_parser_t *ptr) { lxb_css_parser_destroy(ptr, true); });
	}

	if (!css_selectors) {
		auto local = lxb_css_selectors_create();
		if (lxb_css_selectors_init(local, 32) != LXB_STATUS_OK) {
			lxb_css_selectors_destroy(local, true, true);
			goto ensure_failed;
		}
		css_selectors = std::shared_ptr<lxb_css_selectors_t>(
			local, [](lxb_css_selectors_t *ptr) { lxb_css_selectors_destroy(ptr, true, true); });
		lxb_css_parser_selectors_set(css_parser.get(), local);
	}

	if (!selectors) {
		auto local = lxb_selectors_create();
		if (lxb_selectors_init(local) != LXB_STATUS_OK) {
			lxb_selectors_destroy(local, true);
			goto ensure_failed;
		}
		selectors = std::shared_ptr<lxb_selectors_t>(
			local, [](lxb_selectors_t *ptr) { lxb_selectors_destroy(ptr, true); });
	}
	return;

ensure_failed:
	throw std::runtime_error("unable to initialize lexbor");
}

lxb_status_t get_html_cb(const lxb_char_t *data, size_t len, void *ctx) {
	auto out = (std::ostringstream *) ctx;
	*out << std::string_view{(const char *) data, len};
	return LXB_STATUS_OK;
}

lxb_status_t transform_cb(lxb_dom_node_t *node, lxb_css_selector_specificity_t *, void *ctx) {
	auto nodes = (std::vector<lxb_dom_node_t *> *) ctx;
	nodes->push_back(node);
	return LXB_STATUS_OK;
}
}  // namespace

transform_rules utils::compile_transform_rules(const std::vector<transform_rule> &rules) {
	ensure_parsers();

	auto res = std::make_shared<transform_rules_t>();
	for (const auto &rule : rules) {
		auto cselector = (const lxb_char_t *) rule.selector.data();
		auto local = lxb_css_selectors_parse(css_parser.get(), cselector, rule.selector.size());
		if (css_parser.get()->status != LXB_STATUS_OK ||
			lxb_css_log_length(lxb_css_parser_log(css_parser.get()))) {
			throw std::runtime_error("CSS selector `" + rule.selector + "' is invalid");
		}
		res->rules.push_back({
			.selector = local,
			.handle = rule.handle,
		});
	}
	return res;
}

/* ==== utils::html_fragment ==== */
html_fragment::html_fragment(std::string_view html) {
	ensure_parsers();

	root = lxb_html_parse(html_parser.get(), nullptr, 0);
	if (!root) {
		throw std::runtime_error("unable to create HTML document");
	}

	auto body = lxb_html_interface_element(lxb_html_document_body_element(root));
	auto html_data = (const lxb_char_t *) html.data();
	auto elem = lxb_html_element_inner_html_set(body, html_data, html.size());
	if (!elem) {
		throw utils::expected_error("unable to parse HTML fragment");
	}
}

html_fragment::~html_fragment() {
	lxb_html_document_destroy(root);
}

void html_fragment::transform(transform_rules rules, void *ctx) {
	auto body = lxb_dom_interface_node(lxb_html_document_body_element(root));

	for (const auto &rule : rules->rules) {
		std::vector<lxb_dom_node_t *> nodes;
		lxb_selectors_find(selectors.get(), body, rule.selector, transform_cb, &nodes);

		for (auto node : nodes) {
			if (node == body) {
				continue;
			}

			auto next = node->next, parent = node->parent;
			lxb_dom_node_remove(node);
			auto modified = rule.handle(node, ctx);

			if (modified == nullptr) {
				lxb_dom_node_t *child;
				while ((child = node->first_child)) {
					lxb_dom_node_remove(child);
					if (next) {
						lxb_dom_node_insert_before(next, child);
					} else {
						lxb_dom_node_insert_child(parent, child);
					}
				}
			} else {
				if (next) {
					lxb_dom_node_insert_before(next, node);
				} else {
					lxb_dom_node_insert_child(parent, node);
				}
			}
		}
	}
}

std::string html_fragment::get_html() {
	std::ostringstream res;
	auto body = lxb_dom_interface_node(lxb_html_document_body_element(root));
	lxb_html_serialize_tree_cb(body, get_html_cb, &res);
	auto str = res.str();

	assert(str.starts_with("<body>") && str.ends_with("</body>"));
	return str.substr(6, str.size() - 13);
}
