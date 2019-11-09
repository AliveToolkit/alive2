#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <map>
#include <set>

namespace smt {

class DisjointExpr {
  std::map<expr, expr> vals; // val -> domain

public:
  void add(expr &&val, expr &&domain);
  expr operator()() const;
};


class FunctionExpr {
  std::map<expr, expr> fn; // key -> val
  smt::expr empty_val;

public:
  void add(expr &&key, expr &&val);
  void clear();
  void reset(smt::expr &&empty_val);
  expr operator()(expr &key) const;
};

}
