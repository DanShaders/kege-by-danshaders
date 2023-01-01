#include "utils/filter.h"
using namespace utils;

namespace {
char const* EXPR_TYPE_NAME[] = {
    /* [T_ERROR] = */ "<expression error>",
    /* [T_BOOLEAN] = */ "boolean",
    /* [T_INTEGER] = */ "integer",
    /* [T_STRING] = */ "string",
    /* [T_REGEX] = */ "regex",
};

struct token_t {
  // clang-format off
	enum type_t {
		VARIABLE, NUMBER, STRING,
		PAR_OPEN, PAR_CLOSE,
		NOT, AND, OR,
		EQ, NE, GT, GE, LS, LE,
		LIKE,
		// Don't forget about OP_PRIORITY after updating this
	} type = NUMBER;
  // clang-format on

  std::size_t l = -1, r = -1;
  union {
    int64_t s = 0;
    std::size_t u;
  } data;
};

std::vector<token_t> tokenize(std::string_view s, std::ostringstream& err) {
  // Operator which is a prefix of another should be declared later
  auto const OPS = std::initializer_list<std::pair<std::string_view, token_t::type_t>>{
      {"!=", token_t::NE}, {"!", token_t::NOT},   {"&&", token_t::AND}, {"<=", token_t::LE},
      {"<", token_t::LS},  {"==", token_t::EQ},   {">=", token_t::GE},  {">", token_t::GT},
      {"||", token_t::OR}, {"~=", token_t::LIKE},
  };

  std::vector<token_t> tokens;
  for (std::size_t i = 0; i < s.size(); ++i) {
    // Whitespace
    if (isspace(s[i])) {
      continue;
    }

    // Parenthesis
    if (s[i] == '(' || s[i] == ')') {
      tokens.push_back({s[i] == '(' ? token_t::PAR_OPEN : token_t::PAR_CLOSE, i, i + 1});
      continue;
    }

    // Operator
    bool is_operator = false;
    for (auto [op_str, op_type] : OPS) {
      auto op_sz = op_str.size();
      if (i + op_sz <= s.size() && s.substr(i, op_sz) == op_str) {
        tokens.push_back({op_type, i, i + op_sz});
        i += op_sz - 1;
        is_operator = true;
        break;
      }
    }
    if (is_operator) {
      continue;
    }

    // String
    if (s[i] == '"' || s[i] == '\'') {
      auto j = i + 1;
      while (j < s.size()) {
        if (s[j] == s[i] && s[j - 1] != '\\') {
          break;
        }
        ++j;
      }
      if (j == s.size()) {
        err << "Expected " << s[i] << " to close string started at " << i << ", found EOF\n";
      }
      tokens.push_back({token_t::STRING, i + 1, j});
      i = j;
      continue;
    }

    // Number
    if (s[i] == '-' || isdigit(s[i])) {
      int64_t value;
      auto [ptr, ec] = std::from_chars(s.data() + i, s.data() + s.size(), value);
      if (ec == std::errc()) {
        auto j = (std::size_t)(ptr - s.data());
        tokens.push_back({token_t::NUMBER, i, j, {.s = value}});
        i = j - 1;
      } else {
        err << "Number started at index " << i << " cannot be parsed\n";
      }
      continue;
    }

    // Variable
    if (isalpha(s[i]) || s[i] == '_') {
      auto j = i + 1;
      while (j < s.size()) {
        if (s[j] != '_' && !isalnum(s[j])) {
          break;
        }
        ++j;
      }
      tokens.push_back({token_t::VARIABLE, i, j});
      i = j - 1;
      continue;
    }

    err << "No token starts at index " << i << "\n";
  }
  return tokens;
}

int const OP_PRIORITY[] = {
    /* [token_t::VARIABLE] = */ -1,
    /* [token_t::NUMBER] = */ -1,
    /* [token_t::STRING] = */ -1,

    /* [token_t::PAR_OPEN] = */ -1,
    /* [token_t::PAR_CLOSE] = */ -1,

    /* [token_t::NOT] = */ -1,
    /* [token_t::AND] = */ 1,
    /* [token_t::OR] = */ 2,

    /* [token_t::EQ] = */ 0,
    /* [token_t::NE] = */ 0,
    /* [token_t::GT] = */ 0,
    /* [token_t::GE] = */ 0,
    /* [token_t::LS] = */ 0,
    /* [token_t::LE] = */ 0,

    /* [token_t::LIKE] = */ 0,
};

enum vm_op_t : int64_t {
  OP_LOAD_INT64,
  OP_LOAD_VAR,
  OP_LOAD_STR,
  OP_LOAD_REGEX,

  OP_TEST,
  OP_NOT,
  OP_AND,
  OP_OR,
  OP_EQ,
  OP_LS,
  OP_LE,
  OP_S_EQ,
  OP_S_LIKE,
  OP_CJUMP,
  OP_NJUMP,

  OP_HLT,
};

struct expr_build_context;

struct expr_node {
  expr_type_t type;
  token_t const* token;

  expr_node(token_t const* token_) : token(token_) {}

  virtual ~expr_node() {}

  virtual void generate(expr_build_context& ctx, bool regex_required) = 0;
};

using expr_tree = std::shared_ptr<expr_node>;

struct expr_build_context {
  std::string_view s;
  std::ostringstream& err;
  std::vector<token_t>& tokens;
  std::vector<expr_type_t> var_types;

  std::vector<int64_t>& code;
  std::vector<std::string>& data_string;
  std::vector<std::regex>& data_regex;

  void ensure_type(expr_tree operand, expr_type_t type) {
    if (operand->type != type) {
      if (type == T_INTEGER && operand->type == T_BOOLEAN) {
        return;
      } else if (type == T_BOOLEAN && operand->type == T_INTEGER) {
        code.push_back(OP_TEST);
        return;
      }
      err << "Could not convert " << EXPR_TYPE_NAME[operand->type]
          << " (type of expression at index " << operand->token->l << ") to "
          << EXPR_TYPE_NAME[type] << "\n";
    }
  }
};

struct variable_node : public expr_node {
  using expr_node::expr_node;

  void generate(expr_build_context& ctx, bool) override {
    type = ctx.var_types[token->data.u];
    ctx.code.push_back(OP_LOAD_VAR);
    ctx.code.push_back((int64_t) token->data.u);
  }
};

struct integer_node : public expr_node {
  using expr_node::expr_node;

  void generate(expr_build_context& ctx, bool) override {
    type = T_INTEGER;
    ctx.code.push_back(OP_LOAD_INT64);
    ctx.code.push_back(token->data.s);
  }
};

struct string_node : public expr_node {
  using expr_node::expr_node;

  void generate(expr_build_context& ctx, bool regex_required) override {
    std::string unescaped;
    for (std::size_t i = token->l; i < token->r; ++i) {
      if (ctx.s[i] == '\\' && i + 1 != token->r &&
          (ctx.s[i + 1] == '\\' || ctx.s[i + 1] == '\'' || ctx.s[i + 1] == '"')) {
        unescaped += ctx.s[i + 1];
        ++i;
      } else {
        unescaped += ctx.s[i];
      }
    }
    if (regex_required) {
      type = T_REGEX;
      ctx.code.push_back(OP_LOAD_REGEX);
      ctx.code.push_back(ctx.data_regex.size());
      try {
        ctx.data_regex.push_back(std::regex(unescaped, std::regex::icase));
      } catch (std::exception const& e) {
        ctx.err << "Regex at index " << token->l << " is invalid: " << e.what() << "\n";
      }
    } else {
      type = T_STRING;
      ctx.code.push_back(OP_LOAD_STR);
      ctx.code.push_back((int64_t) ctx.data_string.size());
      ctx.data_string.push_back(unescaped);
    }
  }
};

struct error_node : public expr_node {
  using expr_node::expr_node;

  void generate(expr_build_context& ctx, bool) override {
    type = T_ERROR;
    ctx.code.push_back(OP_LOAD_INT64);
    ctx.code.push_back(0);
  }
};

const token_t SHARED_ERROR_TOKEN{};
auto const SHARED_ERROR_NODE = std::make_shared<error_node>(&SHARED_ERROR_TOKEN);

struct unary_operator_node : public expr_node {
  expr_tree t;

  unary_operator_node(token_t* token_, expr_tree t_) : expr_node(token_), t(t_) {}

  void generate(expr_build_context& ctx, bool) override {
    type = T_BOOLEAN;
    t->generate(ctx, false);
    ctx.ensure_type(t, T_BOOLEAN);
    ctx.code.push_back(OP_NOT);
  }
};

struct binary_operator_node : public expr_node {
  expr_tree l, r;

  binary_operator_node(token_t* token_, expr_tree l_, expr_tree r_)
      : expr_node(token_), l(l_), r(r_) {}

  void generate(expr_build_context& ctx, bool) override {
    type = T_BOOLEAN;

    int op = token->type;
    l->generate(ctx, false);

    bool is_and = op == token_t::AND;
    if (is_and || op == token_t::OR) {
      ctx.ensure_type(l, T_BOOLEAN);

      ctx.code.push_back(is_and ? OP_NJUMP : OP_CJUMP);
      auto where = ctx.code.size();
      ctx.code.push_back(0);

      r->generate(ctx, false);
      ctx.ensure_type(r, T_BOOLEAN);

      ctx.code.push_back(is_and ? OP_AND : OP_OR);
      ctx.code[where] = ctx.code.size() - where - 1;
      return;
    }

    r->generate(ctx, op == token_t::LIKE);

    if (op == token_t::EQ || op == token_t::NE) {
      if (l->type == T_STRING) {
        ctx.ensure_type(r, T_STRING);
        ctx.code.push_back(OP_S_EQ);
      } else {
        ctx.ensure_type(l, T_INTEGER);
        ctx.ensure_type(r, T_INTEGER);
        ctx.code.push_back(OP_EQ);
      }
      if (op == token_t::NE) {
        ctx.code.push_back(OP_NOT);
      }
    } else if (op == token_t::LIKE) {
      ctx.ensure_type(l, T_STRING);
      ctx.ensure_type(r, T_REGEX);
      ctx.code.push_back(OP_S_LIKE);
    } else {
      ctx.ensure_type(l, T_INTEGER);
      ctx.ensure_type(r, T_INTEGER);

      if (op == token_t::LE || op == token_t::GT) {
        ctx.code.push_back(OP_LE);
      } else if (op == token_t::LS || op == token_t::GE) {
        ctx.code.push_back(OP_LS);
      }
      if (op == token_t::GT || op == token_t::GE) {
        ctx.code.push_back(OP_NOT);
      }
    }
  }
};

expr_tree build_tree(expr_build_context& ctx, std::size_t l, std::size_t r) {
  if (l == r) {
    if (l == 0) {
      ctx.err << "Empty expression\n";
    } else {
      ctx.err << "Empty expression at indexes " << ctx.tokens[l - 1].l << " .. " << ctx.tokens[r].l
              << "\n";
    }
    return SHARED_ERROR_NODE;
  }

  std::vector<token_t*> op_stack;
  std::vector<expr_tree> expr_stack;

  // There should be unary operator stack, but the only unary operator we have is NOT, so using a
  // shortcut.
  int num_of_not = 0;
  token_t* last_not = nullptr;
  auto push_to_expr_stack = [&](expr_tree tree) {
    if (num_of_not) {
      tree = std::make_shared<unary_operator_node>(last_not, tree);
      num_of_not = 0;
      last_not = nullptr;
    }
    if (expr_stack.size() > op_stack.size()) {
      ctx.err << "Unexpected expression at index " << tree->token->l << "\n";
    } else {
      expr_stack.push_back(tree);
    }
  };

  auto merge_stacks = [&](int priority) {
    while (op_stack.size() && OP_PRIORITY[op_stack.back()->type] < priority) {
      int current = OP_PRIORITY[op_stack.back()->type];
      std::size_t j = 1;
      while (j < op_stack.size() &&
             OP_PRIORITY[op_stack[op_stack.size() - j - 1]->type] == current) {
        ++j;
      }
      auto result = expr_stack[expr_stack.size() - j - 1];
      for (std::size_t i = op_stack.size() - j; i < op_stack.size(); ++i) {
        result = std::make_shared<binary_operator_node>(op_stack[i], result, expr_stack[i + 1]);
      }
      for (std::size_t i = 0; i < j; ++i) {
        op_stack.pop_back();
        expr_stack.pop_back();
      }
      expr_stack.back() = result;
    }
  };

  for (std::size_t i = l; i < r; ++i) {
    auto& token = ctx.tokens[i];
    if (token.type == token_t::VARIABLE) {
      push_to_expr_stack(std::make_shared<variable_node>(&token));
    } else if (token.type == token_t::NUMBER) {
      push_to_expr_stack(std::make_shared<integer_node>(&token));
    } else if (token.type == token_t::STRING) {
      push_to_expr_stack(std::make_shared<string_node>(&token));
    } else if (token.type == token_t::PAR_OPEN) {
      auto j = token.data.u;
      push_to_expr_stack(build_tree(ctx, i + 1, j));
      i = j;
    } else if (token.type == token_t::NOT) {
      num_of_not ^= 1;
      last_not = &token;
    } else if (OP_PRIORITY[token.type] != -1) {
      if (expr_stack.size() == op_stack.size()) {
        ctx.err << "Unexpected operator at index " << token.l << "\n";
      } else {
        merge_stacks(OP_PRIORITY[token.type]);
        op_stack.push_back(&token);
      }
    }
  }
  if (expr_stack.size() == op_stack.size()) {
    if (op_stack.size()) {
      ctx.err << "Unexpected operator at index " << op_stack.back()->l << "\n";
    }
    return SHARED_ERROR_NODE;
  }
  merge_stacks(std::numeric_limits<int>::max());
  return expr_stack[0];
}
}  // namespace

/* ==== utils::filter::impl ==== */
struct filter::impl {
  std::ostringstream err;
  std::vector<int64_t> code;
  std::vector<std::string> data_string;
  std::vector<std::regex> data_regex;
};

/* ==== utils::filter ==== */
filter::filter(std::string_view filter_str,
               std::vector<std::pair<std::string_view, expr_type_t>> const& vars_info)
    : pimpl(new impl{}) {
  // Parse
  auto tokens = tokenize(filter_str, pimpl->err);

  // Compile
  expr_build_context ctx{
      filter_str, pimpl->err, tokens, {}, pimpl->code, pimpl->data_string, pimpl->data_regex,
  };
  std::map<std::string_view, std::size_t> var_id;
  for (std::size_t i = 0; i < vars_info.size(); ++i) {
    var_id[vars_info[i].first] = i;
    ctx.var_types.push_back(vars_info[i].second);
  }

  for (auto& [type, l, r, info] : tokens) {
    if (type == token_t::VARIABLE) {
      auto var_name = filter_str.substr(l, r - l);
      auto it = var_id.find(var_name);
      if (it == var_id.end()) {
        pimpl->err << "Unknown variable ‘" << var_name << "’ at index " << l << "\n";
      } else {
        info.u = it->second;
      }
    }
  }

  std::vector<std::size_t> unclosed;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == token_t::PAR_OPEN) {
      unclosed.emplace_back(i);
    } else if (tokens[i].type == token_t::PAR_CLOSE) {
      if (!unclosed.size()) {
        pimpl->err << "Unmatched ) at index " << tokens[i].l << "\n";
      } else {
        tokens[unclosed.back()].data.u = i;
        unclosed.pop_back();
      }
    }
  }
  for (auto i : unclosed) {
    pimpl->err << "Unmatched ( at index " << tokens[i].l << "\n";
  }
  if (pimpl->err.str().size()) {
    return;
  }

  auto tree = build_tree(ctx, 0, tokens.size());
  tree->generate(ctx, false);
  ctx.ensure_type(tree, T_BOOLEAN);
  ctx.code.push_back(OP_HLT);

  // Link
  for (std::size_t i = 0; i < ctx.code.size(); ++i) {
    if (ctx.code[i] == OP_LOAD_STR) {
      ctx.code[i] = OP_LOAD_INT64;
      ctx.code[i + 1] = (int64_t) &ctx.data_string[size_t(ctx.code[i + 1])];
    } else if (ctx.code[i] == OP_LOAD_REGEX) {
      ctx.code[i] = OP_LOAD_INT64;
      ctx.code[i + 1] = (int64_t) &ctx.data_regex[size_t(ctx.code[i + 1])];
    }
    if (ctx.code[i] == OP_LOAD_INT64 || ctx.code[i] == OP_LOAD_VAR || ctx.code[i] == OP_CJUMP ||
        ctx.code[i] == OP_NJUMP) {
      ++i;
    }
  }
}

filter::~filter() = default;

std::optional<std::string> filter::get_error() const {
  auto err = pimpl->err.str();
  if (err.size()) {
    return {err};
  }
  return {};
}

bool filter::matches(std::vector<int64_t> const& vars) {
  std::vector<int64_t> stack;
  int64_t* pc = &pimpl->code[0];

  while (1) {
    int64_t op = *pc;

    if (op == OP_LOAD_INT64) {
      stack.push_back(*++pc);
    } else if (op == OP_LOAD_VAR) {
      stack.push_back(vars[*++pc]);
    } else if (op == OP_TEST) {
      stack.back() = !!stack.back();
    } else if (op == OP_NOT) {
      stack.back() = !stack.back();
    } else if (op == OP_AND) {
      int64_t prev = stack.back();
      stack.pop_back();
      stack.back() &= prev;
    } else if (op == OP_OR) {
      int64_t prev = stack.back();
      stack.pop_back();
      stack.back() |= prev;
    } else if (op == OP_EQ) {
      int64_t prev = stack.back();
      stack.pop_back();
      stack.back() = stack.back() == prev;
    } else if (op == OP_LS) {
      int64_t prev = stack.back();
      stack.pop_back();
      stack.back() = stack.back() < prev;
    } else if (op == OP_LE) {
      int64_t prev = stack.back();
      stack.pop_back();
      stack.back() = stack.back() <= prev;
    } else if (op == OP_S_EQ) {
      auto const& prev = *(std::string*) stack.back();
      stack.pop_back();
      stack.back() = *((std::string*) stack.back()) == prev;
    } else if (op == OP_S_LIKE) {
      auto const& regex = *(std::regex*) stack.back();
      stack.pop_back();
      auto const& str = *(std::string*) stack.back();
      stack.back() = regex_match(str.begin(), str.end(), regex);
    } else if (op == OP_CJUMP) {
      ++pc;
      if (stack.back()) {
        pc += *pc;
      }
    } else if (op == OP_NJUMP) {
      ++pc;
      if (!stack.back()) {
        pc += *pc;
      }
    } else if (op == OP_HLT) {
      break;
    }
    ++pc;
  }
  return stack.back();
}
