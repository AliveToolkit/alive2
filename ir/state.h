#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <ostream>
#include <set>
#include <unordered_map>

namespace IR {

struct StateValue {
  smt::expr value, non_poison;

  StateValue() {}
  StateValue(smt::expr &&value, smt::expr &&non_poison)
      : value(std::move(value)), non_poison(std::move(non_poison)) {}

  static StateValue mkIf(const smt::expr &cond, const StateValue &then,
                         const StateValue &els);

  friend std::ostream& operator<<(std::ostream &os, const StateValue &val);
};


class Value;
class BasicBlock;
class Function;

class State {
  const Function &f;
  std::set<smt::expr> quantified_vars;
  // var -> ((value, not_poison), domain)
  std::unordered_map<const Value*, StateValue> values;
  std::unordered_map<const BasicBlock*, smt::expr> domain_bbs;
  smt::expr domain;
  smt::expr return_domain;
  // FIXME: replace with disjoint expr builder
  StateValue return_val;
  bool returned = false;

public:
  State(const Function &f);
  void add(const Value &val, StateValue &&e);
  const StateValue& operator[](const Value &val) const;

  bool startBB(const BasicBlock &bb);
  void addJump(const BasicBlock &bb);
  void addReturn(const StateValue &val);
  void addUB(smt::expr &&ub)      { domain &= std::move(ub); }
  void addUB(const smt::expr &ub) { domain &= ub; }
  void addQuantVar(const smt::expr &var);

  auto& getFn() const { return f; }
  const auto& getValues() const { return values; }

  bool fnReturned() const { return returned; }
  const smt::expr& returnDomain() const { return return_domain; }
  const StateValue& returnVal() const { return return_val; }
};

}
