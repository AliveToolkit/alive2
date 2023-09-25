#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <ostream>
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

  auto bits() const { return value.bits(); }

  StateValue zext(unsigned amount) const;
  StateValue trunc(unsigned bw_val, unsigned bw_np) const;
  StateValue zextOrTrunc(unsigned tobw) const;
  StateValue concat(const StateValue &other) const;

  bool isValid() const;

  smt::expr operator==(const StateValue &other) const;
  smt::expr implies(const StateValue &other) const;
  bool eq(const StateValue &other) const;

  std::set<smt::expr> vars() const;
  StateValue subst(const smt::expr &from, const smt::expr &to) const;
  StateValue
    subst(const std::vector<std::pair<smt::expr, smt::expr>> &repls) const;

  StateValue simplify() const;

  auto operator<=>(const StateValue &rhs) const = default;

  friend std::ostream& operator<<(std::ostream &os, const StateValue &val);
};

}
