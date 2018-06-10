// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/function.h"
#include "smt/expr.h"
#include <cassert>

using namespace smt;
using namespace std;

namespace IR {

StateValue StateValue::mkIf(const expr &cond, const StateValue &then,
                            const StateValue &els) {
  return { expr::mkIf(cond, then.value, els.value),
           expr::mkIf(cond, then.non_poison, els.non_poison) };
}

ostream& operator<<(ostream &os, const StateValue &val) {
  return os << val.value << " / " << val.non_poison;
}

State::State(const Function &f) : f(f) {
  domain_bbs.emplace(&f.getBB(""), true);
}

void State::add(const Value &val, StateValue &&e) {
  auto p = values.emplace(&val, move(e));
  assert(p.second);
}

const StateValue& State::operator[](const Value &val) const {
  return values.at(&val);
}

bool State::startBB(const BasicBlock &bb) {
  auto I = domain_bbs.find(&bb);
  if (I == domain_bbs.end())
    return false;

  domain = move(I->second);
  return !domain.isFalse();
}

void State::addJump(const BasicBlock &bb) {
  auto p = domain_bbs.try_emplace(&bb, domain);
  if (!p.second)
    p.first->second |= domain;
}

void State::addReturn(const StateValue &val) {
  if (returned) {
    return_domain |= domain;
    return_val = StateValue::mkIf(domain, val, return_val);
  } else {
    returned = true;
    return_domain = domain;
    return_val = val;
  }
}

void State::addQuantVar(const expr &var) {
  quantified_vars.emplace(var);
}

}
