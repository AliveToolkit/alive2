// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include "util/compiler.h"

using namespace std;

namespace smt {

void AndExpr::add(expr &&e) {
  if (!e.isTrue())
    exprs.insert(move(e));
}

void AndExpr::add(const AndExpr &other) {
  exprs.insert(other.exprs.begin(), other.exprs.end());
}

expr AndExpr::operator()() const {
  return expr::mk_and(exprs);
}

ostream &operator<<(ostream &os, const AndExpr &e) {
  return os << e();
}


void OrExpr::add(expr &&e) {
  if (!e.isFalse())
    exprs.insert(move(e));
}

expr OrExpr::operator()() const {
  return expr::mk_or(exprs);
}

ostream &operator<<(ostream &os, const OrExpr &e) {
  return os << e();
}


void FunctionExpr::add(const expr &key, expr &&val) {
  ENSURE(fn.emplace(key, move(val)).second);
}

void FunctionExpr::add(const FunctionExpr &other) {
  fn.insert(other.fn.begin(), other.fn.end());
}

void FunctionExpr::del(const expr &key) {
  fn.erase(key);
}

expr FunctionExpr::operator()(const expr &key) const {
  DisjointExpr disj(default_val);
  for (auto &[k, v] : fn) {
    disj.add(v, k == key);
  }
  return disj();
}

const expr* FunctionExpr::lookup(const expr &key) const {
  auto I = fn.find(key);
  return I != fn.end() ? &I->second : nullptr;
}

bool FunctionExpr::operator<(const FunctionExpr &rhs) const {
  return tie(fn, default_val) < tie(rhs.fn, rhs.default_val);
}

}
