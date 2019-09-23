#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "ir/type.h"
#include "smt/expr.h"
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace IR {

class Memory;
class State;

class Pointer {
  Memory &m;
  // [offset, local_bid, nonlocal_bid]
  // A pointer is pointing to a local memory block if local_bid is non-zero and
  // nonlocal_bid is zero. A local memory block is a memory block that is
  // allocated by an instruction during the current function call. This does
  // not include allocated blocks from a nested function call. A heap-allocated
  // block can also be a local memory block.
  // Otherwise, a pointer is pointing to a non-local block, which can be either
  // of global variable, heap, or a stackframe that is not this function call.
  // TODO: missing support for address space
  smt::expr p;

  unsigned total_bits() const;
  unsigned bits_for_bids() const;

public:
  Pointer(Memory &m, const char *var_name);
  Pointer(Memory &m, smt::expr p) : m(m), p(std::move(p)) {}
  Pointer(Memory &m, unsigned bid, bool local);
  Pointer(Memory &m, const smt::expr &offset, const smt::expr &local_bid,
          const smt::expr &nonlocal_bid);

  smt::expr is_local() const;

  smt::expr get_bid() const;
  smt::expr get_local_bid() const;
  smt::expr get_nonlocal_bid() const;
  smt::expr get_offset() const;
  smt::expr get_address() const;

  smt::expr block_size() const;

  const smt::expr& operator()() const { return p; }
  smt::expr release() { return std::move(p); }
  unsigned bits() const { return p.bits(); }

  Pointer operator+(unsigned) const;
  Pointer operator+(const smt::expr &bytes) const;
  void operator+=(const smt::expr &bytes);

  smt::expr add_no_overflow(const smt::expr &offset) const;

  smt::expr operator==(const Pointer &rhs) const;
  smt::expr operator!=(const Pointer &rhs) const;

  StateValue sle(const Pointer &rhs) const;
  StateValue slt(const Pointer &rhs) const;
  StateValue sge(const Pointer &rhs) const;
  StateValue sgt(const Pointer &rhs) const;
  StateValue ule(const Pointer &rhs) const;
  StateValue ult(const Pointer &rhs) const;
  StateValue uge(const Pointer &rhs) const;
  StateValue ugt(const Pointer &rhs) const;

  smt::expr inbounds() const;
  smt::expr is_aligned(unsigned align) const;
  void is_dereferenceable(unsigned bytes, unsigned align);
  void is_dereferenceable(const smt::expr &bytes, unsigned align);
  void is_disjoint(const smt::expr &len1, const Pointer &ptr2,
                   const smt::expr &len2) const;
  smt::expr is_block_alive() const;
  smt::expr is_at_heap() const;

  friend std::ostream& operator<<(std::ostream &os, const Pointer &p);
};


class Memory {
  State *state;

  // FIXME: these should be tuned per function
  unsigned bits_for_offset = 64;
  unsigned bits_for_local_bid = 8;
  unsigned bits_for_nonlocal_bid = 8;
  unsigned bits_size_t = 64;

  smt::expr blocks_val; // array: (bid, offset) -> StateValue
  smt::expr blocks_liveness; // array: bid -> uint(1bit), 1 if alive, 0 if freed
  smt::expr blocks_kind; // array: bid -> uint(1bit), 1 if heap, 0 otherwise
  unsigned last_bid = 0;
  unsigned last_idx_ptr = 0;

  std::string mkName(const char *str, bool src) const;
  std::string mkName(const char *str) const;

  smt::expr mk_val_array(const char *name) const;
  smt::expr mk_liveness_uf() const;

public:
  Memory(State &state);

  std::pair<smt::expr, std::vector<smt::expr>> mkInput(const char *name);

  // Allocates a new local memory block.
  smt::expr alloc(const smt::expr &size, unsigned align, bool heap);
  void free(const smt::expr &ptr);

  void store(const smt::expr &ptr, const StateValue &val, Type &type,
             unsigned align);
  StateValue load(const smt::expr &ptr, Type &type, unsigned align);

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytes, unsigned align);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytes, unsigned align_dst, unsigned align_src,
              bool move);

  smt::expr ptr2int(const smt::expr &ptr);
  smt::expr int2ptr(const smt::expr &val);

  static Memory mkIf(const smt::expr &cond, const Memory &then,
                     const Memory &els);

  unsigned bitsOffset() const { return bits_for_offset; }

  friend class Pointer;
};

}
