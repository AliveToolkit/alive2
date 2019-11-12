#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace smt { class FunctionExpr; }

namespace IR {

class Memory;
class State;

class Pointer {
  const Memory &m;

  // [bid, offset]
  // The top bit of bid is 1 if the block is local, 0 otherwise.
  // A local memory block is a memory block that is
  // allocated by an instruction during the current function call. This does
  // not include allocated blocks from a nested function call. A heap-allocated
  // block can also be a local memory block.
  // Otherwise, a pointer is pointing to a non-local block, which can be either
  // of global variable, heap, or a stackframe that is not this function call.
  // TODO: missing support for address space
  smt::expr p;

  unsigned total_bits() const;

  smt::expr get_value(const char *name, const smt::FunctionExpr &fn,
                      const smt::expr &ret_type) const;

public:
  Pointer(const Memory &m, const char *var_name);
  Pointer(const Memory &m, smt::expr p) : m(m), p(std::move(p)) {}
  Pointer(const Memory &m, unsigned bid, bool local);
  Pointer(const Memory &m, const smt::expr &bid, const smt::expr &offset);

  smt::expr is_local() const;

  smt::expr get_bid() const;
  smt::expr get_short_bid() const; // same as get_bid but ignoring is_local bit
  smt::expr get_offset() const;
  smt::expr get_address(bool simplify = true) const;

  smt::expr block_size(bool simplify = true) const;

  const smt::expr& operator()() const { return p; }
  smt::expr short_ptr() const;
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
  void is_dereferenceable(unsigned bytes, unsigned align, bool iswrite);
  void is_dereferenceable(const smt::expr &bytes, unsigned align, bool iswrite);
  void is_disjoint(const smt::expr &len1, const Pointer &ptr2,
                   const smt::expr &len2) const;
  smt::expr is_block_alive() const;
  smt::expr is_readonly() const;

  enum AllocType {
    NON_HEAP,
    MALLOC,
    CXX_NEW,
  };
  smt::expr get_alloc_type() const;

  const Memory& getMemory() const { return m; }

  static Pointer mkNullPointer(const Memory &m);
  smt::expr isNull() const;

  friend std::ostream& operator<<(std::ostream &os, const Pointer &p);
};


class Memory {
  State *state;

  // FIXME: these should be tuned per function
  unsigned bits_for_offset = 64;
  unsigned bits_for_bid = 12;
  // bits_size_t is equivalent to the size of a pointer.
  unsigned bits_size_t = 64;

  bool little_endian;

  smt::expr non_local_block_val;  // array: (bid, offset) -> Byte
  smt::expr local_block_val;

  smt::expr non_local_block_liveness; // array: bid -> bool
  smt::expr local_block_liveness;

  smt::FunctionExpr local_blk_addr;
  smt::FunctionExpr local_blk_size;
  smt::FunctionExpr local_blk_kind;

  smt::expr mk_val_array() const;
  smt::expr mk_liveness_array() const;

public:
  enum BlockKind {
    HEAP, STACK, GLOBAL, CONSTGLOBAL
  };

  Memory(State &state, bool little_endian);

  static void resetGlobalData();
  static void resetLocalBids();

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
                  unsigned *bid_out = nullptr,
                  const smt::expr &precond = smt::expr(true));

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

  unsigned bitsOffset() const { return bits_for_offset; }
  unsigned bitsBid() const { return bits_for_bid; }
  unsigned bitsByte() const { return 1 + 1 + bitsBid() + bitsOffset() + 3; }
  unsigned bitsPtrSize() const { return bits_size_t; }

  // for container use only
  bool operator<(const Memory &rhs) const;

  friend class Pointer;
};

}
