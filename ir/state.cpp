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
  domain_bbs.try_emplace(&f.getFirstBB(), true, set<expr>());
}

const StateValue& State::exec(const Value &v) {
  assert(undef_vars.empty());
  auto val = v.toSMT(*this);
  ENSURE(values_map.try_emplace(&v, (unsigned)values.size()).second);
  values.emplace_back(&v, ValTy(move(val), move(undef_vars)));

  // cleanup potentially used temporary values due to undef rewriting
  while (i_tmp_values > 0) {
    tmp_values[--i_tmp_values] = StateValue();
  }

  return values.back().second.first;
}

const StateValue& State::operator[](const Value &val) {
  auto &[sval, uvars] = values[values_map.at(&val)].second;
  if (uvars.empty())
    return sval;

  vector<pair<expr, expr>> repls;
  for (auto &u : uvars) {
    auto name = UndefValue::getFreshName();
    repls.emplace_back(u, expr::mkVar(name.c_str(), u.bits()));
  }

  auto sval_new = sval.subst(repls);
  if (sval_new.eq(sval)) {
    uvars.clear();
    return sval;
  }

  for (auto &p : repls) {
    undef_vars.emplace(move(p.second));
  }

  return tmp_values[i_tmp_values++] = move(sval_new);
}

const State::ValTy& State::at(const Value &val) const {
  return values[values_map.at(&val)].second;
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  seen_bbs.emplace(&bb);
  auto I = domain_bbs.find(&bb);
  if (I == domain_bbs.end())
    return false;

  domain = move(I->second);
  return !domain.first.isFalse();
}

void State::addJump(const BasicBlock &dst, expr &&cond) {
  if (seen_bbs.count(&dst))
    throw LoopInCFGDetected();

  auto p = domain_bbs.try_emplace(&dst, domain.first && move(cond), undef_vars);
  if (!p.second) {
    p.first->second.first |= move(cond);
    p.first->second.second.insert(undef_vars.begin(), undef_vars.end());
  }
  p.first->second.second.insert(domain.second.begin(), domain.second.end());
}

void State::addJump(const BasicBlock &dst) {
  addJump(dst, true);
  domain.first = false;
}

void State::addJump(StateValue &&cond, const BasicBlock &dst) {
  addJump(dst, move(cond.value) && move(cond.non_poison));
}

void State::addCondJump(const StateValue &cond, const BasicBlock &dst_true,
                        const BasicBlock &dst_false) {
  addJump(dst_true,  cond.value == 1 && cond.non_poison);
  addJump(dst_false, cond.value == 0 && cond.non_poison);
  domain.first = false;
}

void State::addReturn(const StateValue &val) {
  if (returned) {
    return_domain |= domain.first;
    return_val.first = StateValue::mkIf(domain.first, val, return_val.first);
    return_val.second.insert(undef_vars.begin(), undef_vars.end());
    return_val.second.insert(domain.second.begin(), domain.second.end());
  } else {
    returned = true;
    return_domain = move(domain.first);
    return_val = { val, move(undef_vars) };
    return_val.second.insert(domain.second.begin(), domain.second.end());
  }
  domain.first = false;
  undef_vars.clear();
}

void State::addUB(expr &&ub) {
  domain.first &= move(ub);
  domain.second.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(const expr &ub) {
  domain.first &= ub;
  domain.second.insert(undef_vars.begin(), undef_vars.end());
}

void State::addQuantVar(const expr &var) {
  quantified_vars.emplace(var);
}

void State::addUndefVar(const expr &var) {
  undef_vars.emplace(var);
}

void State::resetUndefVars() {
  quantified_vars.insert(undef_vars.begin(), undef_vars.end());
  undef_vars.clear();
}

}
