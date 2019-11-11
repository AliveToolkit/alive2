#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <map>
#include <set>
#include <utility>

namespace smt {

template <typename T>
class DisjointExpr {
  std::map<T, expr> vals; // val -> domain
  std::optional<T> default_val;

public:
  DisjointExpr() {}
  DisjointExpr(const T &default_val) : default_val(default_val) {}
  DisjointExpr(T &&default_val) : default_val(std::move(default_val)) {}

  template <typename V, typename D>
  void add(V &&val, D &&domain) {
    if (domain.isFalse())
      return;
    if (domain.isTrue())
      vals.clear();

    auto [I, inserted] = vals.try_emplace(std::forward<V>(val),
                                          std::forward<D>(domain));
    if (!inserted)
      I->second |= std::forward<D>(domain);
  }

  T operator()() const {
    std::optional<T> ret;
    for (auto &[val, domain] : vals) {
      if (domain.isTrue())
        return val;

      ret = ret ? T::mkIf(domain, val, *ret) : val;
    }
    return ret ? *ret : default_val.value();
  }
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

  // for container use only
  bool operator<(const FunctionExpr &rhs) const;
};

}
