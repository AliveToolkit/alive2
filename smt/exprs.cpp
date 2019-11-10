// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include "ir/state_value.h"
#include "util/compiler.h"

using namespace std;

namespace smt {

template <typename T>
void DisjointExpr<T>::add(T &&val, expr &&domain) {
  if (domain.isFalse())
    return;
  if (domain.isTrue())
    vals.clear();

  auto [I, inserted] = vals.try_emplace(move(val), move(domain));
  if (!inserted)
    I->second |= move(domain);
}

template <typename T>
T DisjointExpr<T>::operator()() const {
  T ret;
  bool first = true;
  for (auto &[val, domain] : vals) {
    if (domain.isTrue())
      return val;

    ret = first ? val : T::mkIf(domain, val, ret);
    first = false;
  }
  if (first)
    return default_val;
  return ret;
}

template class DisjointExpr<expr>;
template class DisjointExpr<IR::StateValue>;


void FunctionExpr::add(expr &&key, expr &&val) {
  ENSURE(fn.emplace(move(key), move(val)).second);
}

void FunctionExpr::add(const FunctionExpr &other) {
  fn.insert(other.fn.begin(), other.fn.end());
}

void FunctionExpr::del(const expr &key) {
  fn.erase(key);
}

expr FunctionExpr::operator()(expr &key) const {
  DisjointExpr disj(default_val);
  for (auto &[k, v] : fn) {
    disj.add(expr(v), k == key);
  }
  return disj();
}

}
