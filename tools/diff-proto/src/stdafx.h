#pragma once

#include <fmt/core.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>

#include <concepts>
#include <map>
#include <set>
#include <vector>

using namespace google::protobuf;
using std::string;
