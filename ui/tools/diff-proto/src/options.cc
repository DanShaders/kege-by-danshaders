#include "options.h"

struct Options {
  bool diffable;
};

Options get_options(const Message &options) {
  Options res{};

  const auto &unknown_fields = options.GetReflection()->GetUnknownFields(options);
  for (int i = 0; i < unknown_fields.field_count(); ++i) {
    const auto &field = unknown_fields.field(i);
    int id = field.number(), type = field.type();

    if (id == 37700 && type == UnknownField::TYPE_VARINT) {
      res.diffable = field.varint();
    }
  }
  return res;
}

bool is_diffable(const Descriptor *msg) {
  return get_options(msg->options()).diffable;
}
