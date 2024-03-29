#include "stdafx.h"

#include "errors.h"
#include "fields.h"
#include "file-context.h"
#include "options.h"
#include "utils.h"

class MessageCodeGenerator : public FileContextHolder {
private:
  Descriptor const* msg;
  std::vector<std::unique_ptr<FieldCodeGenerator>> fields;
  bool has_nonscalars = false;

public:
  MessageCodeGenerator(FileContext const& c_, Descriptor const* msg_)
      : FileContextHolder(c_), msg(msg_) {
    vars["msg"] = msg->name();
    string scope = get_scope(msg->containing_type());
    vars["transport"] = "defs." + scope + msg->name();
    vars["scope"] = scope.size() ? scope.substr(0, scope.size() - 1) : "";

    for_all_fields(msg, [&](FieldDescriptor const* field) {
      if (field->name() == "curr_pos") {
        fields.push_back(std::make_unique<PosFieldCodeGenerator>(c, field));
      } else if (field->name() == "swap_pos") {
        // All of the logic in curr_pos
      } else if (field->name() == "id") {
        fields.push_back(std::make_unique<IDFieldCodeGenerator>(c, field));
      } else if (field->type() != FieldDescriptor::TYPE_MESSAGE) {
        if (field->containing_oneof()) {
          fields.push_back(std::make_unique<ScalarFieldCodeGenerator>(c, field));
        } else if (is_in(field)) {
          fields.push_back(std::make_unique<UnwiredInFieldCodeGenerator>(c, field));
        } else if (is_out(field)) {
          fields.push_back(std::make_unique<UnwiredFieldCodeGenerator>(c, field));
        }
      } else {
        fields.push_back(std::make_unique<SetFieldCodeGenerator>(c, field));
        has_nonscalars = true;
      }
    });
  }

  void begin_scope() {
    if (vars["scope"].size()) {
      println("export namespace $scope$ {");
      indent();
    }
  }

  void generate_delta_type() {
    print("export interface $msg$Delta");
    blockln([&] {
      for (const auto& field : fields) {
        field->generate_delta_entry();
      }
    });
  }

  void generate_context_type() {
    print("type $msg$DiffContext = IContext<Diffable$msg$, $msg$Delta>;\n\n");
  }

  void generate_class_fields() {
    for (auto const& field : fields) {
      field->generate_class_field();
    }
    print(
        "\n"
        "private fields: number = 0;\n"
        "private ctx?: $msg$DiffContext;\n"
        "private msg?: $transport$;\n");
    if (has_nonscalars) {
      println("private supressPropagation = false;");
    }
    println("");
  }

  void generate_constructor() {
    print("constructor(msg: $transport$)");
    blockln([&] {
      println("this.msg = msg.clone();");
      for (const auto& field : fields) {
        field->generate_constructor_assignment();
      }
    });
  }

  void generate_getters_setters() {
    for (auto const& field : fields) {
      field->generate_getter_setter();
    }
  }

  void generate_local_creation() {
    print(
        "createLocal(onDeltaChange: DeltaChangeCallback): Diffable$msg$ {\n"
        "  const local = this.clone();\n"
        "  local.mount({ remote: this, delta: {}, onDeltaChange: onDeltaChange });\n"
        "  return local;\n"
        "}\n\n");
  }

  void generate_clone() {
    print(
        "clone(): Diffable$msg$ {\n"
        "  const result = new Diffable$msg$(nonNull(this.msg));\n"
        "  this.msg = undefined;\n"
        "  return result;\n"
        "}\n\n");
  }

  void generate_mount() {
    print("mount(ctx: $msg$DiffContext): void");
    blockln([&] {
      print("this.ctx = ctx;\n");
      for (const auto& field : fields) {
        field->generate_mount();
      }
    });
  }

  void generate_synchronize() {
    print(
        "synchronize(): [syncObj: $transport$, commitDelta: $msg$Delta] | undefined {\n"
        "  if (!this.fields) { return undefined; }\n"
        "  nonNull(this.ctx);\n"
        "  return [this.serialize(), structuredClone(this.ctx!.delta)];\n"
        "}\n");
  }

  void generate_commit() {
    print("commit(commitDelta: $msg$Delta): void");
    blockln([&] {
      print(
          "const remote = this.ctx!.remote, delta = this.ctx!.delta;\n"
          "let saveFields = this.fields;\n");
      if (has_nonscalars) {
        println("this.supressPropagation = true;");
      }

      for (const auto& field : fields) {
        field->generate_commit();
      }

      if (has_nonscalars) {
        println("this.supressPropagation = false;");
      }
      print(
          "if (this.fields !== saveFields) {\n"
          "  this.ctx!.onDeltaChange(this.fields, this.fields - saveFields);\n"
          "}\n");
    });
  }

  void generate_serialize() {
    print("serialize(): $transport$");
    blockln([&] {
      print(
          "const delta = this.ctx!.delta;\n"
          "const obj = new $transport$();\n");
      for (const auto& field : fields) {
        field->generate_serialize();
      }
      println("return obj;");
    });
  }

  void generate_class() {
    print(
        "export class Diffable$msg$ implements IDiffable<Diffable$msg$, $msg$Delta, "
        "$transport$>");
    block([&] {
      generate_class_fields();
      generate_constructor();
      generate_getters_setters();
      generate_clone();
      generate_commit();
      generate_local_creation();
      generate_mount();
      generate_serialize();
      generate_synchronize();
    });
  }

  void end_scope() {
    if (vars["scope"].size()) {
      outdent();
      println("}");
    }
    println("");
  }
};

void FileContext::generate_header() const {
  p->Print(R"(/* eslint-disable */
import structuredClone from "@ungap/structured-clone";
import * as defs from "./$name$_pb";
import { assert, nonNull } from "utils/assert";
import {
  DeltaChangeCallback,
  DiffableSet,
  IContext,
  IDiffable,
  areBuffersEqual,
} from "utils/diff";

)",
           "name", compiler::StripProto(file->name()));
}

void FileContext::generate_message(Descriptor const* msg) const {
  MessageCodeGenerator gen{*this, msg};
  gen.begin_scope();
  gen.generate_delta_type();
  gen.generate_context_type();
  gen.generate_class();
  gen.end_scope();
}

class Generator : public compiler::CodeGenerator {
private:
  friend FileContext;

  mutable std::set<FileDescriptor const*> files_set;
  mutable std::map<FileDescriptor const*, FileContext> file_ctx;
  mutable std::set<Descriptor const*> diffable_msgs;
  mutable std::string* error;
  mutable compiler::GeneratorContext* context;

  void add_error(auto cause, std::string const& message) const {
    FileDescriptor const* file = cause->file();
    SourceLocation location;
    cause->GetSourceLocation(&location);
    *error += fmt::format("{}:{}:{}: {}\n", file->name(), location.start_line + 1,
                          location.start_column + 1, message);
  }

  void mark_diffable(Descriptor const* msg) const {
    diffable_msgs.insert(msg);
    if (!file_ctx.contains(msg->file())) {
      file_ctx.insert({msg->file(), FileContext(msg->file(), this)});
    }
    for_all_fields(msg, [&](FieldDescriptor const* field) {
      if (field->is_map()) {
        return;
      }
      auto type = field->message_type();
      if (type && !is_diffable(type)) {
        if (!files_set.contains(type->file())) {
          add_error(field, ERR_OUT_OF_SCOPE_DIFFABLE);
        }
        mark_diffable(type);
      }
    });
  }

  void check_message(Descriptor const* msg) const {
    bool has_id = false;
    for_all_fields(msg, [&](FieldDescriptor const* field) {
      if (field->name() == "id") {
        has_id = true;
        if (field->cpp_type() >= 5) {
          add_error(field, ERR_NONINTEGRAL_ID);
        }
      }
      if (field->is_map()) {
        add_error(field, ERR_MAP_DISALLOWED);
      } else if (field->real_containing_oneof()) {
        add_error(field, ERR_ONEOF_DISALLOWED);
      } else if (field->type() == FieldDescriptor::TYPE_GROUP) {
        add_error(field, ERR_GROUP_DISALLOWED);
      } else if (!field->is_repeated() != !field->message_type()) {
        add_error(field, ERR_REPEATED_SCALARS_DISALLOWED);
      }
    });
    if (!has_id) {
      add_error(msg, ERR_NO_ID);
    }
  }

public:
  bool GenerateAll(std::vector<const FileDescriptor*> const& files, std::string const&,
                   compiler::GeneratorContext* generator_context,
                   std::string* error_) const override {
    files_set = {files.begin(), files.end()};
    diffable_msgs.clear();
    error = error_;
    context = generator_context;

    for (auto file : files) {
      for (int i = 0; i < file->message_type_count(); ++i) {
        for_all_nested(file->message_type(i), [this](Descriptor const* msg) {
          if (!is_diffable(msg)) {
            return;
          }
          mark_diffable(msg);
        });
      }
    }

    for (auto msg : diffable_msgs) {
      check_message(msg);
    }
    if (!error->empty()) {
      file_ctx.clear();
      return false;
    }

    for (auto const& [_, file] : file_ctx) {
      file.generate_header();
    }
    for (auto msg : diffable_msgs) {
      file_ctx.find(msg->file())->second.generate_message(msg);
    }

    // Destroy all io::Printer's before leaving GenerateAll
    file_ctx.clear();
    return true;
  }

  bool Generate(FileDescriptor const* file, string const& param,
                compiler::GeneratorContext* generator_context, string* error_) const override {
    return GenerateAll({file}, param, generator_context, error_);
  }

  uint64_t GetSupportedFeatures() const override {
    return FEATURE_PROTO3_OPTIONAL;
  }
};

FileContext::FileContext(FileDescriptor const* file_, Generator const* generator_)
    : file(file_),
      generator(generator_),
      p(std::make_unique<io::Printer>(
          generator->context->Open(compiler::StripProto(file->name()) + "_pb_diff.ts"), '$')) {}

int main(int argc, char** argv) {
  Generator generator;
  return compiler::PluginMain(argc, argv, &generator);
}
