#include "fields.h"

ScalarFieldCodeGenerator::ScalarFieldCodeGenerator(FileContext const& c_,
                                                   FieldDescriptor const* field_)
    : FieldCodeGenerator(c_, field_) {
  vars["fname"] = "f" + uppercased;
  vars["type"] = TYPE_REMAP[field->type()];
  vars["setter"] = "set" + uppercased;
  vars["getter"] =
      "get" + uppercased + (field->type() == FieldDescriptor::TYPE_BYTES ? "_asU8" : "");
}

void ScalarFieldCodeGenerator::generate_delta_entry() {
  println("$name$?: $type$;");
}

void ScalarFieldCodeGenerator::generate_class_field() {
  println("private $fname$: $type$;");
}

void ScalarFieldCodeGenerator::generate_constructor_assignment() {
  println("this.$fname$ = msg.$getter$();");
}

void ScalarFieldCodeGenerator::generate_getter_setter() {
  print("get $name$(): $type$");
  blockln([&] { println("return this.$fname$;"); });

  print("set $name$(value: $type$)");
  blockln([&] {
    print(
        "this.$fname$ = value;\n"
        "if (!this.ctx) { return; }\n");
    if (field->type() == FieldDescriptor::TYPE_BYTES) {
      println("if (areBuffersEqual(this.ctx.remote.$fname$, value)) {");
    } else {
      println("if (this.ctx.remote.$fname$ === value) {");
    }
    print(
        "  if (this.ctx.delta.$name$ !== undefined) {\n"
        "    delete this.ctx.delta.$name$;\n"
        "    this.ctx.onDeltaChange(--this.fields, -1);\n"
        "  }\n"
        "} else {\n"
        "  if (this.ctx.delta.$name$ === undefined) {\n"
        "    this.ctx.delta.$name$ = value;\n"
        "    this.ctx.onDeltaChange(++this.fields, 1);\n"
        "  } else {\n"
        "    this.ctx.delta.$name$ = value;\n"
        "    this.ctx.onDeltaChange(this.fields, 0);\n"
        "  }\n"
        "}\n");
  });
}

void ScalarFieldCodeGenerator::generate_commit() {
  print(
      "if (commitDelta.$name$ !== undefined) {\n"
      "  remote.$fname$ = commitDelta.$name$;\n"
      "  if (delta.$name$ === undefined) {\n"
      "    ++this.fields;\n"
      "    delta.$name$ = this.$fname$;\n"
      "  } else {\n");
  if (field->type() == FieldDescriptor::TYPE_BYTES) {
    println("    if (areBuffersEqual(this.$fname$, remote.$fname$)) {");
  } else {
    println("    if (this.$fname$ === remote.$fname$) {");
  }
  print(
      "      --this.fields;\n"
      "      delete delta.$name$;\n"
      "    }\n"
      "  }\n"
      "}\n");
}

void ScalarFieldCodeGenerator::generate_serialize() {
  print(
      "if (delta.$name$ !== undefined) {\n"
      "  obj.$setter$(delta.$name$);\n"
      "}\n");
}
