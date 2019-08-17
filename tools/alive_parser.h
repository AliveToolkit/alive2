#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include <string>
#include <string_view>
#include <vector>

namespace IR { class Type; }

namespace tools {

std::vector<Transform> parse(std::string_view buf);
IR::Type& get_sym_type();

struct parser_initializer {
  parser_initializer();
  ~parser_initializer();
};


struct ParseException {
  std::string str;
  unsigned lineno;

  ParseException(std::string &&str, unsigned lineno)
    : str(std::move(str)), lineno(lineno) {}
};

constexpr unsigned PARSER_READ_AHEAD = 16;

}
