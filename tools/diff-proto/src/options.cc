#include "options.h"

struct Options {
  bool diffable, in, out;
};

Options get_options(Message const& options) {
  Options res{};

  auto const& unknown_fields = options.GetReflection()->GetUnknownFields(options);
  for (int i = 0; i < unknown_fields.field_count(); ++i) {
    auto const& field = unknown_fields.field(i);
    int id = field.number(), type = field.type();

    if (id == 37700 && type == UnknownField::TYPE_VARINT) {
      res.diffable = field.varint();
    } else if (id == 37701 && type == UnknownField::TYPE_VARINT) {
      res.in = field.varint();
    } else if (id == 37702 && type == UnknownField::TYPE_VARINT) {
      res.out = field.varint();
    }
  }
  return res;
}

bool is_diffable(Descriptor const* msg) {
  return get_options(msg->options()).diffable;
}

bool is_in(FieldDescriptor const* field) {
  return get_options(field->options()).in;
}

bool is_out(FieldDescriptor const* field) {
  return get_options(field->options()).out;
}
