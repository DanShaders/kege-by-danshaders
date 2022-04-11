#include "fields.h"

ScalarFieldCodeGenerator::ScalarFieldCodeGenerator(const FileContext &c_,
                                                   const FieldDescriptor *field_)
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
  blockln([&] {
    println("return this.$fname$;");
  });

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
        "  delete this.ctx.delta.$name$;\n"
        "  this.ctx.onDeltaChange(--this.fields, -1);\n"
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

void ScalarFieldCodeGenerator::generate_prepend_delta() {
  print(
      "if (delta.$name$ !== undefined) {\n"
      "  if (ctx.delta.$name$ === undefined) {\n"
      "    ctx.delta.$name$ = delta.$name$;\n"
      "    ++this.fields;\n"
      "  } else if (");
  if (field->type() == FieldDescriptor::TYPE_BYTES) {
    print("areBuffersEqual(ctx.delta.$name$, delta.$name$)");
  } else {
    print("ctx.delta.$name$ === delta.$name$");
  }
  print(
      ") {\n"
      "    delete ctx.delta.$name$;\n"
      "    --this.fields;\n"
      "  }\n"
      "}\n");
}

void ScalarFieldCodeGenerator::generate_apply_delta() {
  print(
      "if (delta.$name$ !== undefined) {\n"
      "  rollback.$name$ = this.$fname$;\n"
      "  this.$fname$ = delta.$name$;\n"
      "}\n");
}

void ScalarFieldCodeGenerator::generate_serialize() {
  print(
      "if (delta.$name$ !== undefined) {\n"
      "  obj.$setter$(delta.$name$);\n"
      "  delete delta.$name$;\n"
      "}\n");
}
