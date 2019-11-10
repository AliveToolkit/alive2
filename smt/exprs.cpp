// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include "util/compiler.h"

using namespace std;

namespace smt {

void DisjointExpr::add(expr &&val, expr &&domain) {
  if (domain.isFalse())
    return;
  if (domain.isTrue())
    vals.clear();

  auto [I, inserted] = vals.try_emplace(move(val), move(domain));
  if (!inserted)
    I->second |= move(domain);
}

expr DisjointExpr::operator()() const {
  expr ret;
  bool first = true;
  for (auto &[val, domain] : vals) {
    if (domain.isTrue())
      return val;

    ret = first ? val : expr::mkIf(domain, val, ret);
    first = false;
  }
  if (first)
    return default_val;
  return ret;
}


void FunctionExpr::add(expr &&key, expr &&val) {
  ENSURE(fn.emplace(move(key), move(val)).second);
}

void FunctionExpr::add(const FunctionExpr &other) {
  fn.insert(other.fn.begin(), other.fn.end());
}

void FunctionExpr::del(const expr &key) {
  fn.erase(key);
}

void FunctionExpr::reset(expr &&val) {
  fn.clear();
  default_val = move(val);
}

expr FunctionExpr::operator()(expr &key) const {
  DisjointExpr disj(default_val);
  for (auto &[k, v] : fn) {
    disj.add(expr(v), k == key);
  }
  return disj();
}

}
