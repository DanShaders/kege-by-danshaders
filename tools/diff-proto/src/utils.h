#pragma once

#include "stdafx.h"

inline string uppercase_first(std::string_view const& s) {
  string res;
  res += char(std::toupper(s[0]));
  res += s.substr(1);
  return res;
}

inline string get_scope(Descriptor const* message) {
  if (!message) {
    return "";
  }
  return get_scope(message->containing_type()) + message->name() + ".";
}

inline void for_all_nested(Descriptor const* msg,
                           const std::invocable<Descriptor const*> auto& callback) {
  callback(msg);
  for (int i = 0; i < msg->nested_type_count(); ++i) {
    for_all_nested(msg->nested_type(i), callback);
  }
}

inline void for_all_fields(Descriptor const* msg,
                           const std::invocable<FieldDescriptor const*> auto& callback) {
  for (int i = 0; i < msg->field_count(); ++i) {
    callback(msg->field(i));
  }
}

inline char const* TYPE_REMAP[] = {nullptr,  "number", "number",     "number",  "number",
                                   "number", "number", "number",     "boolean", "string",
                                   nullptr,  "any",    "Uint8Array", "number",  nullptr,
                                   "number", "number", "number",     "number"};
