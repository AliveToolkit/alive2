#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>
#include <string_view>

namespace tools {

enum token {
#define TOKEN(x) x,
#include "tools/tokens.h"
#undef TOKEN
};

struct yylval_t {
  union {
    uint64_t num;
    double fp_num;
    std::string_view str;
  };
  yylval_t() {}
};

void yylex_init(std::string_view str);
token yylex();

extern yylval_t yylval;
extern unsigned yylineno;
extern const char *const token_name[];

struct LexException {
  std::string str;
  unsigned lineno;

  LexException(std::string &&str, unsigned lineno)
    : str(std::move(str)), lineno(lineno) {}
};


constexpr unsigned LEXER_READ_AHEAD = 20;

}
