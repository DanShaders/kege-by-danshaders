#pragma once

#include "stdafx.h"

inline string uppercase_first(const std::string_view &s) {
  string res;
  res += char(std::toupper(s[0]));
  res += s.substr(1);
  return res;
}

inline string get_scope(const Descriptor *message) {
  if (!message) {
    return "";
  }
  return get_scope(message->containing_type()) + message->name() + ".";
}

inline void for_all_nested(const Descriptor *msg,
                           const std::invocable<const Descriptor *> auto &callback) {
  callback(msg);
  for (int i = 0; i < msg->nested_type_count(); ++i) {
    for_all_nested(msg->nested_type(i), callback);
  }
}

inline void for_all_fields(const Descriptor *msg,
                           const std::invocable<const FieldDescriptor *> auto &callback) {
  for (int i = 0; i < msg->field_count(); ++i) {
    callback(msg->field(i));
  }
}

inline const char *TYPE_REMAP[] = {nullptr,  "number", "number",     "number",  "number",
                                   "number", "number", "number",     "boolean", "string",
                                   nullptr,  "any",    "Uint8Array", "number",  nullptr,
                                   "number", "number", "number",     "number"};
