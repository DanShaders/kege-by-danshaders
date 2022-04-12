#include "fields.h"

UnwiredFieldCodeGenerator::UnwiredFieldCodeGenerator(const FileContext &c_,
                                                   const FieldDescriptor *field_)
    : FieldCodeGenerator(c_, field_) {
  vars["type"] = TYPE_REMAP[field->type()];
  vars["setter"] = "set" + uppercased;
}

void UnwiredFieldCodeGenerator::generate_class_field() {
  println("$name$?: $type$;");
}

void UnwiredFieldCodeGenerator::generate_serialize() {
  print(
    "if (this.$name$ !== undefined) {\n"
    "  obj.$setter$(this.$name$);\n"
    "}\n");
}
