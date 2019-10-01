#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "ir/type.h"
#include "smt/expr.h"
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace IR {

class Memory;
class State;

class Pointer {
  const Memory &m;
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
  Pointer(const Memory &m, const char *var_name);
  Pointer(const Memory &m, smt::expr p) : m(m), p(std::move(p)) {}
  Pointer(const Memory &m, unsigned bid, bool local);
  Pointer(const Memory &m, const smt::expr &offset, const smt::expr &local_bid,
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

  const Memory &getMemory() const { return m; }

  // Makes a null pointer.
  // TODO: add a bool flag that says whether the twin memory model is used.
  // In the twin memory model, a null pointer is a physical pointer with
  // absolute address 0.
  static Pointer mkNullPointer(Memory &m);

  friend std::ostream& operator<<(std::ostream &os, const Pointer &p);
};


class Memory {
  State *state;

  // FIXME: these should be tuned per function
  unsigned bits_for_offset = 64;
  unsigned bits_for_local_bid = 8;
  unsigned bits_for_nonlocal_bid = 8;
  // bits_size_t is equivalent to the size of a pointer.
  unsigned bits_size_t = 64;

  smt::expr blocks_val; // array: (bid, offset) -> Byte
  smt::expr blocks_liveness; // array: bid -> uint(1bit), 1 if alive, 0 if freed
  smt::expr blocks_kind; // array: bid -> uint(1bit), 1 if heap, 0 otherwise
  // last_bid stores 1 + the last memory block id.
  // Block id 0 is reserved for a null block.
  // TODO: In the twin memory, there is no null block, so the initial last_bid
  // should depend on whether the twin memory is used or not.
  // When setting last_bid to 0 to support twin memory, be careful with local
  // id because last_bid is also used to set local bid but local bid cannot be
  // 0.
  unsigned last_bid = 1;
  unsigned last_idx_ptr = 0;

  std::string mkName(const char *str, bool src) const;
  std::string mkName(const char *str) const;

  smt::expr mk_val_array(const char *name) const;
  smt::expr mk_liveness_uf() const;

public:
  enum BlockKind {
    HEAP, STACK, GLOBAL
  };

  Memory(State &state);

  std::pair<smt::expr, std::vector<smt::expr>> mkInput(const char *name);

  // Allocates a new memory block.
  // If bid is not specified, it creates a fresh block id by increasing
  // last_bid.
  // If bid is specified, the bid is used, and last_bid is not increased.
  // In this case, it is caller's responsibility to give a unique bid, and
  // bumpLastBid() should be called in advance to correctly do this.
  // The newly assigned bid is stored to bid_out if bid_out != nullptr.
  smt::expr alloc(const smt::expr &size, unsigned align, BlockKind blockKind,
                  std::optional<unsigned> bid = std::nullopt,
                  unsigned *bid_out = nullptr);

  void free(const smt::expr &ptr);

  void store(const smt::expr &ptr, const StateValue &val, const Type &type,
             unsigned align);
  StateValue load(const smt::expr &ptr, const Type &type, unsigned align);

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytesize, unsigned align);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytesize, unsigned align_dst, unsigned align_src,
              bool move);

  smt::expr ptr2int(const smt::expr &ptr);
  smt::expr int2ptr(const smt::expr &val);

  static Memory mkIf(const smt::expr &cond, const Memory &then,
                     const Memory &els);

  // Returns the number of bits needed to represent a pointer offset.
  unsigned bitsOffset() const { return bits_for_offset; }
  // Returns the number of bits needed to represent a block id.
  unsigned bitsBid() const { return bits_for_local_bid +
                                    bits_for_nonlocal_bid; }
  // Returns the number of bits needed to represent a byte.
  unsigned bitsByte() const { return 1 + 1 + bitsBid() + bitsOffset() + 3; }
  // Returns the number of bits needed to represent a pointer.
  unsigned bitsPtrSize() const { return bits_size_t; }
  void bumpLastBid(unsigned bid);

  friend class Pointer;
};

}
