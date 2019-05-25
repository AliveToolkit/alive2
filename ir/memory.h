#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "smt/expr.h"

namespace IR {

class State;

class Memory {
  State &state;

  // FIXME: these should be tuned per function
  unsigned bits_for_offset = 32;
  unsigned bits_for_local_bid = 8;
  unsigned bits_for_nonlocal_bid = 8;
  unsigned bits_size_t = 64;

  smt::expr blocks_size; // array: bid -> size in bytes
  smt::expr blocks_val;  // array: (bid, offset) -> StateValue

public:
  Memory(State &state);

  void store(const smt::expr &ptr, const StateValue &val);
  StateValue load(const smt::expr &ptr, unsigned bits);

  static Memory ite(const smt::expr &cond, const Memory &then,
                    const Memory &els);
};

}
