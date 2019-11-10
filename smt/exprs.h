#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <map>
#include <set>

namespace smt {

template <typename T>
class DisjointExpr {
  std::map<T, expr> vals; // val -> domain
  T default_val;

public:
  DisjointExpr() {}
  DisjointExpr(const T &default_val) : default_val(default_val) {}
  DisjointExpr(T &&default_val) : default_val(std::move(default_val)) {}

  void add(T &&val, expr &&domain);
  T operator()() const;
};


class FunctionExpr {
  std::map<expr, expr> fn; // key -> val
  expr default_val;

public:
  FunctionExpr() {}
  FunctionExpr(expr &&default_val) : default_val(std::move(default_val)) {}
  void add(expr &&key, expr &&val);
  void add(const FunctionExpr &other);
  void del(const expr &key);
  expr operator()(expr &key) const;
};

}
