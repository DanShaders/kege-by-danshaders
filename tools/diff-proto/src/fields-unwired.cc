#include "fields.h"

UnwiredFieldCodeGenerator::UnwiredFieldCodeGenerator(FileContext const& c_,
                                                     FieldDescriptor const* field_)
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

UnwiredInFieldCodeGenerator::UnwiredInFieldCodeGenerator(FileContext const& c_,
                                                         FieldDescriptor const* field_)
    : FieldCodeGenerator(c_, field_) {
  vars["type"] = TYPE_REMAP[field->type()];
  vars["getter"] =
      "get" + uppercased + (field->type() == FieldDescriptor::TYPE_BYTES ? "_asU8" : "");
}

void UnwiredInFieldCodeGenerator::generate_class_field() {
  println("$name$: $type$;");
}

void UnwiredInFieldCodeGenerator::generate_constructor_assignment() {
  println("this.$name$ = msg.$getter$();");
}
