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
  domain_bbs.emplace(&f.getFirstBB(), true);
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

  if (undef_vars.empty()) {
    undef_vars = uvars;
    return sval;
  }

  // if evaluated expr shares undef vars with previously evaluated expr, rename
  // those undef vars
  vector<pair<expr, expr>> repls;
  auto VI = uvars.begin(), VE = uvars.end();
  for (auto I = undef_vars.begin(), E = undef_vars.end(); I != E && VI != VE;) {
    if (I->eq(*VI)) {
      auto name = UndefValue::getFreshName();
      repls.emplace_back(*I, expr::mkVar(name.c_str(), I->bits()));
      ++I, ++VI;

    } else if (*I < *VI) {
      ++I;

    } else {
      undef_vars.emplace_hint(I, *VI);
      ++VI;
    }
  }
  undef_vars.insert(VI, VE);

  if (repls.empty())
    return sval;

  for (auto &p : repls) {
    undef_vars.emplace(p.second);
  }

  tmp_values[i_tmp_values] =
      { sval.value.subst(repls), sval.non_poison.subst(repls) };
  return tmp_values[i_tmp_values++];
}

const State::ValTy& State::at(const Value &val) const {
  return values[values_map.at(&val)].second;
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
    // TODO: combine undef_vars
    return_val = { StateValue::mkIf(domain, val, return_val.first),
                   move(undef_vars) };
  } else {
    returned = true;
    return_domain = domain;
    return_val = { val, move(undef_vars) };
  }
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
