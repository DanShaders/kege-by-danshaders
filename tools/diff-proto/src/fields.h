#pragma once

#include "stdafx.h"

#include "file-context.h"
#include "utils.h"

class FieldCodeGenerator : public FileContextHolder {
protected:
  FieldDescriptor const* field;
  std::string name, uppercased;

public:
  FieldCodeGenerator(FileContext const& c_, FieldDescriptor const* field_)
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
  ScalarFieldCodeGenerator(FileContext const& c_, FieldDescriptor const* field_);

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

class UnwiredFieldCodeGenerator : public FieldCodeGenerator {
public:
  UnwiredFieldCodeGenerator(FileContext const& c_, FieldDescriptor const* field_);

  void generate_class_field() override;
  void generate_serialize() override;
};

class UnwiredInFieldCodeGenerator : public FieldCodeGenerator {
public:
  UnwiredInFieldCodeGenerator(FileContext const& c_, FieldDescriptor const* field_);

  void generate_class_field() override;
  void generate_constructor_assignment() override;
};

class SetFieldCodeGenerator : public FieldCodeGenerator {
public:
  SetFieldCodeGenerator(FileContext const& c_, FieldDescriptor const* field_);

  void generate_delta_entry() override;
  void generate_class_field() override;
  void generate_constructor_assignment() override;
  void generate_mount() override;
  void generate_commit() override;
  void generate_serialize() override;
};

class PosFieldCodeGenerator : public ScalarFieldCodeGenerator {
public:
  using ScalarFieldCodeGenerator::ScalarFieldCodeGenerator;

  void generate_serialize() override;
};
