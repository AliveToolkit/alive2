// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include "util/compiler.h"
#include <vector>

using namespace std;

namespace smt {

void AndExpr::add(expr &&e) {
  if (!e.isTrue())
    exprs.insert(move(e));
}

void AndExpr::add(const AndExpr &other) {
  exprs.insert(other.exprs.begin(), other.exprs.end());
}

void AndExpr::del(const AndExpr &other) {
  for (auto &e : other.exprs)
    exprs.erase(e);
}

bool AndExpr::contains(const expr &e) const {
  return exprs.count(e);
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


template<> DisjointExpr<expr>::DisjointExpr(const expr &e, bool unpack_ite) {
  assert(unpack_ite);
  vector<pair<expr, expr>> worklist = { {e, true} };
  expr cond, then, els;

  do {
    auto [v, c] = worklist.back();
    worklist.pop_back();
    if (v.isIf(cond, then, els)) {
      worklist.emplace_back(move(then), c && cond);
      worklist.emplace_back(move(els), c && !cond);
    } else {
      add(move(v), move(c));
    }
  } while (!worklist.empty());
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

optional<expr> FunctionExpr::operator()(const expr &key) const {
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

FunctionExpr FunctionExpr::simplify() const {
  FunctionExpr newfn;
  if (default_val)
    newfn.default_val = default_val->simplify();

  for (auto &[k, v] : fn) {
    newfn.add(k.simplify(), v.simplify());
  }
  return newfn;
}

bool FunctionExpr::operator<(const FunctionExpr &rhs) const {
  return tie(fn, default_val) < tie(rhs.fn, rhs.default_val);
}

ostream& operator<<(ostream &os, const FunctionExpr &f) {
  os << "{\n";
  for (auto &[k, v] : f) {
    os << k << ": " << v << '\n';
  }
  if (f.default_val)
    os << "default: " << *f.default_val << '\n';
  return os << '}';
}

}
