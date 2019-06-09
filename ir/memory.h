#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "smt/expr.h"
#include <string>
#include <utility>
#include <vector>

namespace IR {

class Memory;
class State;

class Pointer {
  Memory &m;
  // [offset, local_bid, nonlocal_bid]
  smt::expr p;

  unsigned bits_for_bids() const;

public:
  Pointer(Memory &m, smt::expr p) : m(m), p(std::move(p)) {}
  Pointer(Memory &m, unsigned bid, bool local);

  smt::expr get_bid() const;
  smt::expr get_offset() const;
  smt::expr get_address() const;

  const smt::expr& operator()() const { return p; }
  smt::expr&& release() { return std::move(p); }
  unsigned bits() const { return p.bits(); }

  void operator++(void);
  Pointer operator+(unsigned) const;
  Pointer operator+(const smt::expr &bytes) const;
  void operator+=(const smt::expr &bytes);

  smt::expr ult(const Pointer &rhs) const;
  smt::expr uge(const Pointer &rhs) const;

  smt::expr inbounds() const;
  smt::expr is_aligned(unsigned align) const;
  void is_dereferenceable(unsigned bytes, unsigned align);
  void is_dereferenceable(const smt::expr &bytes, unsigned align);
};


class Memory {
  State *state;

  // FIXME: these should be tuned per function
  unsigned bits_for_offset = 32;
  unsigned bits_for_local_bid = 8;
  unsigned bits_for_nonlocal_bid = 8;
  unsigned bits_size_t = 64;

  smt::expr blocks_size; // array: bid -> size in bytes
  smt::expr blocks_val;  // array: (bid, offset) -> StateValue
  unsigned last_bid = 0;
  unsigned last_idx_ptr = 0;

  std::string mkName(const char *str) const;
  smt::expr block_addr(const smt::expr &bid) const;

public:
  Memory(State &state);

  std::pair<smt::expr, std::vector<smt::expr>> mkInput(const char *name);

  smt::expr alloc(const smt::expr &bytes, unsigned align, bool local);
  void free(const smt::expr &ptr);

  void store(const smt::expr &ptr, const StateValue &val, unsigned align);
  StateValue load(const smt::expr &ptr, unsigned bits, unsigned align);

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytes, unsigned align);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytes, unsigned align_dst, unsigned align_src);

  smt::expr ptr2int(const smt::expr &ptr);
  smt::expr int2ptr(const smt::expr &val);

  static Memory mkIf(const smt::expr &cond, const Memory &then,
                     const Memory &els);

  unsigned bitsOffset() const { return bits_for_offset; }

  friend class Pointer;
};

}
