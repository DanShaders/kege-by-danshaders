#pragma once

#include "stdafx.h"

#include "KEGE.h"

namespace logging {
void init();
void set_thread_name(const std::string_view &);

void info(const std::string_view &);
void warn(const std::string_view &);
void error(const std::string_view &);
void admin(const std::string_view &);

#ifdef KEGE_LOG_DEBUG_ENABLED
void debug(const std::string_view &);
#else
inline void debug(const std::string_view &) {}
#endif
}  // namespace logging