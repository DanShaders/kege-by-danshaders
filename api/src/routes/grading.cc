#include "routes/grading.h"

#include "task-types.pb.h"

double routes::check_and_grade(std::string_view user_answer, std::string_view jury_answer,
							   int grading_policy) {
	if (grading_policy == api::FULL_MATCH_CASE_SENSITIVE) {
		return user_answer == jury_answer;
	} else if (grading_policy == api::FULL_MATCH) {
		if (user_answer.size() != jury_answer.size()) {
			return 0;
		}
		for (size_t i = 0; i < user_answer.size(); ++i) {
			if (tolower(static_cast<unsigned char>(user_answer[i])) !=
				tolower(static_cast<unsigned char>(jury_answer[i]))) {
				return 0;
			}
		}
		return 1;
	} else {
		auto split = [](std::string_view s) {
			std::vector<std::string_view> result;
			for (size_t i = 0; i < s.size(); ++i) {
				size_t j = i;
				while (j < s.size() && s[j]) {
					++j;
				}
				result.push_back(s.substr(i, j - i));
				i = j;
			}
			return result;
		};

		std::vector<std::string_view> user_parts = split(user_answer),
									  jury_parts = split(jury_answer);

		if (grading_policy == api::INDEPENDENT) {
			size_t matches = 0;
			for (size_t i = 0; i < std::min(user_parts.size(), jury_parts.size()); ++i) {
				if (user_parts[i] == jury_parts[i]) {
					++matches;
				}
			}
			return static_cast<double>(matches) / static_cast<double>(jury_parts.size());
		} else if (grading_policy == api::INDEPENDENT_SWAP_PENALTY) {
			if (user_parts.size() != 2 || jury_parts.size() != 2) {
				return 0;
			}
			if (user_parts[0] == jury_parts[0] && user_parts[1] == jury_parts[1]) {
				return 1;
			}
			if (user_parts[0] == jury_parts[1] && user_parts[1] == jury_parts[0]) {
				return 0.5;
			}
			if (user_parts[0] == jury_parts[0] || user_parts[1] == jury_parts[1]) {
				return 0.5;
			}
			return 0;
		}
	}
	return 0;
}
