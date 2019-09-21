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

  StateValue concat(const StateValue &other) const;
  smt::expr both() const { return value && non_poison; }

  bool eq(const StateValue &other) const;

  StateValue
    subst(const std::vector<std::pair<smt::expr, smt::expr>> &repls) const;

  friend std::ostream& operator<<(std::ostream &os, const StateValue &val);
};

}
