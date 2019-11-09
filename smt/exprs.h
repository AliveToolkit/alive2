#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <map>
#include <set>

namespace smt {

class DisjointExpr {
  std::map<expr, expr> vals; // val -> domain
  expr default_val;

public:
  DisjointExpr() {}
  DisjointExpr(const expr &default_val)
    : default_val(default_val) {}

  void add(expr &&val, expr &&domain);
  expr operator()() const;
};


class FunctionExpr {
  std::map<expr, expr> fn; // key -> val
  expr default_val;

public:
  void add(expr &&key, expr &&val);
  void clear();
  void reset(smt::expr &&default_val);
  expr operator()(expr &key) const;
};

}
