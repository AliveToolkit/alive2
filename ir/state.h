#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <array>
#include <ostream>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

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
public:
  using ValTy = std::pair<StateValue, std::set<smt::expr>>;

private:
  const Function &f;
  std::set<smt::expr> quantified_vars;

  // var -> ((value, not_poison), undef_vars)
  std::unordered_map<const Value*, unsigned> values_map;
  std::vector<std::pair<const Value*, ValTy>> values;

  std::unordered_map<const BasicBlock*, smt::expr> domain_bbs;

  // temp state
  smt::expr domain;
  std::set<smt::expr> undef_vars;
  std::array<StateValue, 4> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  smt::expr return_domain;
  // FIXME: replace with disjoint expr builder
  ValTy return_val;
  bool returned = false;

public:
  State(const Function &f);

  const StateValue& exec(const Value &v);
  const StateValue& operator[](const Value &val);
  const ValTy& at(const Value &val) const;

  bool startBB(const BasicBlock &bb);
  void addJump(const BasicBlock &bb);
  void addReturn(const StateValue &val);
  void addUB(smt::expr &&ub)      { domain &= std::move(ub); }
  void addUB(const smt::expr &ub) { domain &= ub; }
  void addQuantVar(const smt::expr &var);
  void addUndefVar(const smt::expr &var);
  void resetUndefVars();

  auto& getFn() const { return f; }
  const auto& getValues() const { return values; }
  const auto& getQuantVars() const { return quantified_vars; }

  bool fnReturned() const { return returned; }
  auto& returnDomain() const { return return_domain; }
  auto& returnVal() const { return return_val; }
};

}
