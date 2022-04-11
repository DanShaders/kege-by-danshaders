#include "fields.h"

void IDFieldCodeGenerator::generate_class_field() {
  println("readonly id: number;");
}

void IDFieldCodeGenerator::generate_constructor_assignment() {
  println("this.id = msg.getId();");
}

void IDFieldCodeGenerator::generate_serialize() {
  println("obj.setId(this.id);");
}
