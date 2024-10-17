#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "smt/expr.h"

namespace IR {

struct PtrInput {
  unsigned idx = 0;
  StateValue val;
  smt::expr byval = smt::expr::mkUInt(0, 1);
  smt::expr noread = false;
  smt::expr nowrite = false;
  smt::expr nocapture = false;

  PtrInput(unsigned idx, StateValue &&val, smt::expr &&byval,
           smt::expr &&noread, smt::expr &&nowrite, smt::expr &&nocapture) :
    idx(idx), val(std::move(val)), byval(std::move(byval)),
    noread(std::move(noread)), nowrite(std::move(nowrite)),
    nocapture(std::move(nocapture)) {}

  PtrInput(const StateValue &val) : val(val) {}
  PtrInput(const smt::expr &val) : val(StateValue(smt::expr(val), true)) {}

  smt::expr implies(const PtrInput &rhs) const;
  smt::expr implies_attrs(const PtrInput &rhs) const;
  auto operator<=>(const PtrInput &rhs) const = default;
};

}
