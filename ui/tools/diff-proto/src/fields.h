#pragma once

#include "file-context.h"
#include "stdafx.h"
#include "utils.h"

class FieldCodeGenerator : public FileContextHolder {
protected:
  const FieldDescriptor *field;
  std::string name, uppercased;

public:
  FieldCodeGenerator(const FileContext &c_, const FieldDescriptor *field_)
      : FileContextHolder(c_), field(field_) {
    name = field->camelcase_name();
    uppercased = uppercase_first(name);
    vars["name"] = name;
  }

  virtual void generate_delta_entry() {}
  virtual void generate_class_field() {}
  virtual void generate_constructor_assignment() {}
  virtual void generate_getter_setter() {}
  virtual void generate_mount() {}
  virtual void generate_commit() {}
  virtual void generate_serialize() {}
};

class ScalarFieldCodeGenerator : public FieldCodeGenerator {
public:
  ScalarFieldCodeGenerator(const FileContext &c_, const FieldDescriptor *field_);

  void generate_delta_entry() override;
  void generate_class_field() override;
  void generate_constructor_assignment() override;
  void generate_getter_setter() override;
  void generate_commit() override;
  void generate_serialize() override;
};

class IDFieldCodeGenerator : public FieldCodeGenerator {
public:
  using FieldCodeGenerator::FieldCodeGenerator;

  void generate_class_field() override;
  void generate_constructor_assignment() override;
  void generate_serialize() override;
};
