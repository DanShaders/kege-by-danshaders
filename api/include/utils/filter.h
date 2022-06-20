#pragma once

#include "stdafx.h"

namespace utils {
enum expr_type_t {
	T_ERROR,
	T_BOOLEAN,
	T_INTEGER,
	T_STRING,
	T_REGEX,
	// Don't forget about EXPR_TYPE_NAME after updating this
};

class filter {
	ONLY_DEFAULT_MOVABLE_CLASS(filter)

private:
	struct impl;
	std::unique_ptr<impl> pimpl;

public:
	filter(std::string_view filter_str,
		   const std::vector<std::pair<std::string_view, expr_type_t>> &vars_info);
	~filter();

	std::optional<std::string> get_error() const;

	bool matches(const std::vector<int64_t> &vars);
};
}  // namespace utils
