#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <libpq-fe.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <set>
#include <vector>

const std::map<Oid, std::string_view> type_mapping = {
    {16, "bool"},
    {17, "std::string"},
    {20, "int64_t"},
    {23, "int"},
    {25, "std::string"},
    {701, "double"},
    {1016, "std::vector<int64_t>"},
    {1042, "std::string"},
    {1114, "async::pq::timestamp"},
};

template <typename T>
class scope_guard {
private:
  T func;

public:
  scope_guard(T func_) : func(func_) {}

  ~scope_guard() {
    func();
  }
};

void trim(std::string_view &s) {
  auto start = std::ranges::find_if_not(s, static_cast<int (*)(int)>(std::isspace));
  auto end =
      std::ranges::find_if_not(std::views::reverse(s), static_cast<int (*)(int)>(std::isspace))
          .base();
  if (end < start) {
    s = {};
  } else {
    s = {start, end};
  }
}

struct sql_statement {
  int lineno;
  std::string name;
  std::set<std::string> nullable_fields;

  std::string sql_command;
};

int main(int argc, char **argv) {
  if (argc < 3) {
    fmt::print(stderr, "Usage: {} [database connection string] [filename]\n", argv[0]);
    return 1;
  }

  std::ifstream input_file(argv[2]);
  if (!input_file) {
    fmt::print(stderr, "Fatal: unable to open input file\n");
    return 1;
  }

  auto db = PQconnectdb(argv[1]);
  scope_guard db_guard([&] {
    PQfinish(db);
  });

  if (PQstatus(db) != CONNECTION_OK) {
    fmt::print(stderr, "Fatal: unable to connect to the database\n");
    return 1;
  }

  std::string line_str;
  std::vector<sql_statement> statements;

  bool has_errors = false;
  int lineno = 0;
  auto issue_error = [&](std::string_view msg) {
    has_errors = true;
    trim(msg);
    fmt::print(stderr, "{}:{}: {}: {}\n", argv[2], lineno,
               format(fg(fmt::terminal_color::red), "error"), msg);
  };

  auto check_in_statement = [&] {
    if (!statements.size()) {
      issue_error("there is no statement here to specify it");
      return false;
    }
    return true;
  };

  while (std::getline(input_file, line_str)) {
    ++lineno;

    std::string_view line = line_str;
    trim(line);
    if (line == "") {
      continue;
    }

    if (line.starts_with("-- ")) {
      if (line.starts_with("--  ")) {
        // Treat as statement specification
        if (!check_in_statement()) {
          continue;
        }

        const static std::string NULLABLE_START = "--  nullable ";
        if (!line.starts_with(NULLABLE_START)) {
          issue_error("unknown statement specification");
        } else {
          statements.back().nullable_fields.emplace(line.substr(NULLABLE_START.size()));
        }
      } else {
        // Treat as new statement declaration
        std::string name{line.substr(3)};
        for (char &c : name) {
          c = (char) std::toupper(c);
          if (c == ' ') {
            c = '_';
          }
        }
        statements.push_back({.lineno = lineno, .name = name});
      }
    } else {
      if (!check_in_statement()) {
        continue;
      }

      statements.back().sql_command += line;
      statements.back().sql_command += '\n';
    }
  }

  std::string output = R"(#if __INCLUDE_LEVEL__ != 0
#include <async/pq.h>

)";

  for (auto &stmt : statements) {
    lineno = stmt.lineno;

    std::string command;
    std::string next_argument;
    std::vector<std::string> args;
    int in_param = false;
    for (char c : stmt.sql_command) {
      if (c == '`') {
        in_param ^= 1;
        if (in_param == 0) {
          command += fmt::format("${}", args.size() + 1);
          args.push_back(next_argument);
          next_argument.clear();
        }
      } else {
        (in_param ? next_argument : command) += c;
      }
    }
    if (in_param) {
      issue_error("unmatched `");
    }
    args.insert(args.begin(), "sql_queries::" + stmt.name + "_QUERY");

    auto prepare_result = PQprepare(db, "", command.c_str(), 0, nullptr);
    if (PQresultStatus(prepare_result) != PGRES_COMMAND_OK) {
      std::string formatted_stmt = "";
      int stmt_lineno = 0;
      for (char c : command) {
        if (formatted_stmt.empty() || formatted_stmt.back() == '\n') {
          ++stmt_lineno;
          formatted_stmt += format(fg(fmt::color::light_blue), "{:2}", stmt_lineno);
          formatted_stmt += " | ";
        }
        formatted_stmt += c;
      }
      issue_error(
          fmt::format("cannot prepare statement\n{}{}", formatted_stmt, PQerrorMessage(db)));
    } else {
      auto query_stats = PQdescribePrepared(db, "");
      if (PQresultStatus(query_stats) != PGRES_COMMAND_OK) {
        issue_error(fmt::format("Unexpected error while interacting with database\n{}",
                                PQerrorMessage(db)));
      } else {
        int params_len = PQnparams(query_stats), result_len = PQnfields(query_stats);
        std::vector<std::string> input_type_seq, output_type_seq;

        for (int i = 0; i < params_len; ++i) {
          auto oid = PQparamtype(query_stats, i);

          auto it = type_mapping.find(oid);
          if (it == type_mapping.end()) {
            issue_error(fmt::format("Unknown type of parameter `{}` with oid={}", args[i], oid));
          } else {
            input_type_seq.emplace_back(it->second);
          }
        }

        for (int i = 0; i < result_len; ++i) {
          auto oid = PQftype(query_stats, i);
          auto name = PQfname(query_stats, i);

          auto it = type_mapping.find(oid);
          if (it == type_mapping.end()) {
            issue_error(fmt::format("Unknown type of column {} with oid={}", name, oid));
          } else {
            if (stmt.nullable_fields.count(name)) {
              output_type_seq.push_back("std::optional<" + std::string(it->second) + ">");
            } else {
              output_type_seq.emplace_back(it->second);
            }
          }
        }

        fmt::format_to(std::back_inserter(output), R"EOF(namespace sql_queries {{
const async::pq::prepared_sql_query<
  async::pq::type_sequence<{}>,
  async::pq::type_sequence<{}>
> {}_QUERY{{R"BoRdEr({})BoRdEr"}};
}}

#define {}_REQUEST {}

)EOF",
                       fmt::join(input_type_seq, ", "), fmt::join(output_type_seq, ", "), stmt.name,
                       command, stmt.name, fmt::join(args, ", "));
      }
    }
  }

  output += "#endif";

  if (has_errors) {
    return 1;
  }

  std::ofstream output_file(argv[2] + std::string(".cc"));
  output_file << output;
}
