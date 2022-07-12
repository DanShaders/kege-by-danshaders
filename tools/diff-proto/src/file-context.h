#pragma once

#include "stdafx.h"

class Generator;

struct FileContext {
  const FileDescriptor *file;
  const Generator *generator;
  std::unique_ptr<io::Printer> p;

  FileContext(const FileDescriptor *file_, const Generator *generator_);
  void generate_header() const;
  void generate_message(const Descriptor *msg) const;
};

#define println(x) print(x "\n")

class FileContextHolder {
protected:
  const FileContext &c;
  std::map<string, string> vars;

  FileContextHolder(const FileContext &c_) : c(c_) {}

  void print(char const *s) {
    c.p->Print(vars, s);
  }

  void block(std::invocable<> auto body) {
    c.p->Print(" {\n");
    c.p->Indent();
    body();
    c.p->Outdent();
    c.p->Print("}\n");
  }

  void blockln(std::invocable<> auto body) {
    block(body);
    c.p->Print("\n");
  }

  void ifelse(std::invocable<> auto then, std::invocable<> auto otherwise) {
    c.p->Print(" {\n");
    c.p->Indent();
    then();
    c.p->Outdent();
    c.p->Print("} else {\n");
    c.p->Indent();
    otherwise();
    c.p->Outdent();
    c.p->Print("}\n");
  }

  void indent() {
    c.p->Indent();
  }

  void outdent() {
    c.p->Outdent();
  }
};
