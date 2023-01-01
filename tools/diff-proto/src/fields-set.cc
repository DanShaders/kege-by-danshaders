#include "fields.h"

SetFieldCodeGenerator::SetFieldCodeGenerator(FileContext const& c_, FieldDescriptor const* field_)
    : FieldCodeGenerator(c_, field_) {
  auto type = field->message_type();
  string type_name = type->name();
  string scope = get_scope(type->containing_type());

  vars["diffable"] = scope + "Diffable" + type_name;
  vars["delta"] = scope + type_name + "Delta";
  vars["transport"] = "defs." + scope + type_name;
  vars["adder"] = "add" + uppercased;
  vars["getter"] = "get" + uppercased + "List";
}

void SetFieldCodeGenerator::generate_delta_entry() {
  println("$name$?: Map<number, $delta$>;");
}

void SetFieldCodeGenerator::generate_class_field() {
  println("readonly $name$: DiffableSet<$diffable$, $delta$, $transport$>;");
}

void SetFieldCodeGenerator::generate_constructor_assignment() {
  println(
      "this.$name$ = new DiffableSet(\n"
      "  msg.$getter$(),\n"
      "  obj => new $diffable$(obj)\n"
      ");");
}

void SetFieldCodeGenerator::generate_mount() {
  print(
      "{\n"
      "  const curr = new Map<number, $delta$>();\n"
      "  this.$name$.mount({\n"
      "    remote: ctx.remote.$name$,\n"
      "    delta: curr,\n"
      "    onDeltaChange: (fields: number, fieldsDelta: number): void => {\n"
      "      if (!fields && fieldsDelta < 0) {\n"
      "        delete ctx.delta.$name$;\n"
      "        --this.fields;\n"
      "        this.supressPropagation || ctx.onDeltaChange(this.fields, -1);\n"
      "      } else if (fields === fieldsDelta) {\n"
      "        ctx.delta.$name$ = curr;\n"
      "        ++this.fields;\n"
      "        this.supressPropagation || ctx.onDeltaChange(this.fields, 1);\n"
      "      } else {\n"
      "        this.supressPropagation || ctx.onDeltaChange(this.fields, 0);\n"
      "      }\n"
      "    },\n"
      "  });\n"
      "}\n");
}

void SetFieldCodeGenerator::generate_commit() {
  print(
      "if (commitDelta.$name$ !== undefined) {\n"
      "  this.$name$.commit(commitDelta.$name$);\n"
      "}\n");
}

void SetFieldCodeGenerator::generate_serialize() {
  println("this.$name$.serialize(msg => obj.$adder$(msg));");
}
