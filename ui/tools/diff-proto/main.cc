#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>

#include <vector>
using namespace google::protobuf;

const char *TYPE_REMAP[] = {nullptr,  "number", "number",     "number",  "number",
							"number", "number", "number",     "boolean", "string",
							nullptr,  nullptr,  "Uint8Array", "number",  nullptr,
							"number", "number", "number",     "number"};

enum DiffFieldType { NONE = 0, ID = 1, SET = 2, ARRAY = 3 };

struct Options {
	bool gen_diff;
	bool diffable;
	DiffFieldType type;
};

Options get_options(const Message &options) {
	Options res{};
	const auto &unknown_fields = options.GetReflection()->GetUnknownFields(options);
	for (int i = 0; i < unknown_fields.field_count(); ++i) {
		const auto &field = unknown_fields.field(i);
		if (field.type()) {
			continue;
		}
		int num = field.number();
		if (num == 37700) {
			res.gen_diff = field.varint();
		} else if (num == 37701) {
			uint64_t val = field.varint();
			if (val > 3) {
				val = 0;
			}
			res.type = DiffFieldType(val);
		} else if (num == 37702) {
			res.diffable = field.varint();
		}
	}
	return res;
}

std::string snake_to_camel(const std::string_view &s) {
	std::string t;
	for (std::size_t i = 0; i < s.size(); ++i) {
		if (s[i] == '_' && i + 1 != s.size() && std::isalpha(s[i + 1])) {
			t += char(std::toupper(s[i + 1]));
			++i;
		} else {
			t += s[i];
		}
	}
	return t;
}

std::string uppercase_first(const std::string_view &s) {
	std::string res;
	res += char(std::toupper(s[0]));
	res += s.substr(1);
	return res;
}

struct Msg {
	struct Field {
		const FieldDescriptor *field;
		std::string_view name, getter, setter, vname, jstype;
		std::map<std::string, std::string> vars;
		DiffFieldType diff_type;
		FieldDescriptor::Type type;

		Field(const FieldDescriptor *field_, std::map<std::string, std::string> vars_)
			: field(field_), vars(vars_) {
			diff_type = get_options(field->options()).type;
			type = field->type();
			jstype = vars["type"] = TYPE_REMAP[type];
			name = vars["field"] = snake_to_camel(field->name());
			if (type != FieldDescriptor::TYPE_BYTES) {
				getter = vars["getter"] = "get" + uppercase_first(name);
			} else {
				getter = vars["getter"] = "get" + uppercase_first(name) + "_asU8";
			}
			setter = vars["setter"] = "set" + uppercase_first(name);
			if (diff_type == ID) {
				vname = vars["vname"] = name;
			} else {
				vname = vars["vname"] = "v" + uppercase_first(name);
			}
		}
	};

	const Descriptor *message;
	std::vector<Field> fields;
	std::string_view name;
	std::map<std::string, std::string> vars;

	Msg(const Descriptor *message_) : message(message_) {
		name = vars["msg"] = message->name();
		for (int i = 0; i < message->field_count(); ++i) {
			fields.push_back(Field(message->field(i), vars));
		}
	}
};

template <typename T>
void block(io::Printer &p, const T &func, bool newLine = false, bool skipNewLine = false) {
	p.Print(" {\n");
	p.Indent();
	func();
	p.Outdent();
	if (skipNewLine) {
		p.Print("} ");
	} else {
		p.Print("}\n");
	}
	if (newLine) {
		p.Print("\n");
	}
}

template <typename T1, typename T2>
void ifelse(io::Printer &p, const T1 &then, const T2 &otherwise) {
	p.Print(" {\n");
	p.Indent();
	then();
	p.Outdent();
	p.Print("} else {\n");
	p.Indent();
	otherwise();
	p.Outdent();
	p.Print("}\n");
}

class Generator : public compiler::CodeGenerator {
private:
	void GenerateDiffType(const Msg &message, io::Printer &p) const {
		p.Print(message.vars, "export type $msg$Delta =");
		block(
			p,
			[&] {
				for (const auto &field : message.fields) {
					if (field.diff_type == NONE) {
						p.Print(field.vars, "$field$?: $type$;\n");
					}
				}
			},
			true);
	}

	void GenerateClassFields(const Msg &message, io::Printer &p) const {
		for (const auto &field : message.fields) {
			if (field.diff_type == ID) {
				p.Print(field.vars, "readonly $field$: $type$;\n");
			} else {
				p.Print(field.vars, "private $vname$: $type$;\n");
			}
		}
		p.Print(message.vars, "\nctx?: $msg$DiffContext;\n\n");
	}

	void GenerateConstructor(const Msg &message, io::Printer &p) const {
		p.Print(message.vars, "constructor(aMessage: defs.$msg$)");
		block(
			p,
			[&] {
				for (const auto &field : message.fields) {
					if (field.diff_type == ID) {
						p.Print(field.vars, "this.$field$ = aMessage.$getter$();\n");
					} else {
						p.Print(field.vars, "this.$vname$ = aMessage.$getter$();\n");
					}
				}
			},
			true);
	}

	void GenerateGetterSetters(const Msg &message, io::Printer &p) const {
		for (const auto &field : message.fields) {
			if (field.diff_type == ID) {
				continue;
			}

			p.Print(field.vars, "get $field$(): $type$");
			block(
				p,
				[&] {
					p.Print(field.vars, "return this.$vname$;\n");
				},
				true);

			p.Print(field.vars, "set $field$(value: $type$)");
			block(
				p,
				[&] {
					p.Print(field.vars, "this.$vname$ = value;\n");
					p.Print("if (!this.ctx) { return; }\n");
					if (field.type == FieldDescriptor::TYPE_BYTES) {
						p.Print(field.vars, "if (areBuffersEqual(this.ctx.remote.$vname$, value))");
					} else {
						p.Print(field.vars, "if (this.ctx.remote.$vname$ === value)");
					}
					ifelse(
						p,
						[&] {
							p.Print(field.vars, "delete this.ctx.delta.$field$;\n");
						},
						[&] {
							p.Print(field.vars, "this.ctx.delta.$field$ = value;\n");
							p.Print("this.ctx.cbDeltaChange();\n");
						});
				},
				true);
		}
	}

	void GenerateMemberFunctions(const Msg &message, io::Printer &p) const {
		// applyDelta
		p.Print(
			message.vars,
			"static applyDelta(obj: $msg$Delta | Diffable$msg$, delta: $msg$Delta): $msg$Delta");
		block(
			p,
			[&] {
				p.Print(message.vars, "const backwardDelta: $msg$Delta = {};\n");

				for (const auto &field : message.fields) {
					if (field.diff_type == ID) {
						continue;
					}

					p.Print(field.vars, "if (delta.hasOwnProperty(\"$field$\"))");
					block(p, [&] {
						p.Print(field.vars, "backwardDelta.$field$ = obj.$field$;\n");
						p.Print(field.vars, "if (delta.$field$ === undefined)");
						ifelse(
							p,
							[&] {
								p.Print(field.vars, "delete obj.$field$;\n");
							},
							[&] {
								p.Print(field.vars, "obj.$field$ = delta.$field$;\n");
							});
					});
				}

				p.Print("return backwardDelta;\n");
			},
			true);

		p.Print(message.vars, "applyDelta(delta: $msg$Delta): $msg$Delta");
		block(
			p,
			[&] {
				p.Print(message.vars, "return Diffable$msg$.applyDelta(this, delta);\n");
			},
			true);

		// createLocal
		p.Print(message.vars, "createLocal(cbDeltaChange: () => void): Diffable$msg$");
		block(
			p,
			[&] {
				p.Print(message.vars, "const msg = new defs.$msg$();\n");
				for (const auto &field : message.fields) {
					p.Print(field.vars, "msg.$setter$(this.$vname$);\n");
				}
				p.Print(message.vars, "const local = new Diffable$msg$(msg);\n");
				p.Print(
					"local.ctx = {local: local, remote: this, delta: {}, cbDeltaChange: "
					"cbDeltaChange};\n");
				p.Print("return local;\n");
			},
			true);

		// serializeDelta
		p.Print(message.vars, "serializeDelta(delta: $msg$Delta): defs.$msg$");
		block(p, [&] {
			p.Print(message.vars, "const obj = new defs.$msg$();\n");
			for (const auto &field : message.fields) {
				if (field.diff_type == ID) {
					p.Print(field.vars, "obj.$setter$(this.$field$);\n");
				} else {
					p.Print(field.vars, "if (delta.hasOwnProperty(\"$field$\"))");
					block(p, [&] {
						p.Print(field.vars, "assert(delta.$field$ !== undefined);\n");
						p.Print(field.vars, "obj.$setter$(delta.$field$);\n");
					});
				}
			}
			p.Print("return obj;\n");
		});
	}

	void GenerateClass(const Msg &message, io::Printer &p) const {
		p.Print(message.vars, "type $msg$DiffContext =");
		block(
			p,
			[&] {
				p.Print(message.vars, "local: Diffable$msg$;\n");
				p.Print(message.vars, "remote: Diffable$msg$;\n");
				p.Print(message.vars, "delta: $msg$Delta;\n");
				p.Print(message.vars, "cbDeltaChange: () => void;\n");
			},
			true);

		p.Print(message.vars, "export class Diffable$msg$");
		block(
			p,
			[&] {
				GenerateClassFields(message, p);
				GenerateConstructor(message, p);
				GenerateGetterSetters(message, p);
				GenerateMemberFunctions(message, p);
			},
			true);
	}

	void GenerateFor(const Descriptor *message, io::Printer &p) const {
		if (!get_options(message->options()).diffable) {
			return;
		}
		Msg msg{message};
		GenerateDiffType(msg, p);
		GenerateClass(msg, p);
	}

public:
	bool Generate(const FileDescriptor *file, const string &, compiler::GeneratorContext *context,
				  string *) const {
		if (!get_options(file->options()).gen_diff) {
			return true;
		}
		auto stream = context->Open(compiler::StripProto(file->name()) + "_pb_diff.ts");
		auto p = io::Printer(stream, '$');
		p.Print("/* eslint-disable */\n");
		p.Print("import * as defs from \"./$name$_pb\";\n", "name",
				compiler::StripProto(file->name()));
		p.Print("import { assert } from \"utils/assert\";\n");
		p.Print("import { areBuffersEqual } from \"utils/common\";\n");
		p.Print("\n");
		for (int i = 0; i < file->message_type_count(); ++i) {
			GenerateFor(file->message_type(i), p);
		}
		return true;
	}

	uint64_t GetSupportedFeatures() const override {
		return FEATURE_PROTO3_OPTIONAL;
	}
};

int main(int argc, char **argv) {
	Generator generator;
	return compiler::PluginMain(argc, argv, &generator);
}
