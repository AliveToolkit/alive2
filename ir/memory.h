#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state_value.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <optional>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace smt { class Model; }

namespace IR {

class Memory;
class Pointer;
class State;


// A data structure that represents a byte.
// A byte is either a pointer byte or a non-pointer byte.
// Pointer byte's representation:
//   +-+-----------+-----------------------------+---------------+---------+
//   |1|non-poison?|  Pointer (see class below)  | byte offset   | padding |
//   | |(1 bit)    |                             | (0 or 3 bits) |         |
//   +-+-----------+-----------------------------+---------------+---------+
// Non-pointer byte's representation:
//   +-+--------------------+--------------------+-------------------------+
//   |0| non-poison bit(s)  | data               |         padding         |
//   | | (bits_byte)        | (bits_byte)        |                         |
//   +-+--------------------+--------------------+-------------------------+

class Byte {
  const Memory &m;
  smt::expr p;

public:
  // Creates a byte with its raw representation.
  Byte(const Memory &m, smt::expr &&byterepr);

  // Creates a pointer byte that represents i'th byte of p.
  // non_poison should be an one-bit vector or boolean.
  Byte(const Pointer &ptr, unsigned i, const smt::expr &non_poison);

  // Creates a non-pointer byte that has data and non_poison.
  // data and non_poison should have bits_byte bits.
  Byte(const Memory &m, const smt::expr &data, const smt::expr &non_poison);

  smt::expr isPtr() const;
  smt::expr ptrNonpoison() const;
  Pointer ptr() const;
  smt::expr ptrValue() const;
  smt::expr ptrByteoffset() const;
  smt::expr nonptrNonpoison() const;
  smt::expr nonptrValue() const;
  smt::expr isPoison(bool fullbit = true) const;
  smt::expr isZero() const; // zero or null

  const smt::expr& operator()() const { return p; }

  smt::expr operator==(const Byte &rhs) const {
    return p == rhs.p;
  }

  static unsigned bitsByte();

  static Byte mkPtrByte(const Memory &m, const smt::expr &val);
  static Byte mkNonPtrByte(const Memory &m, const smt::expr &val);
  static Byte mkPoisonByte(const Memory &m);
  friend std::ostream& operator<<(std::ostream &os, const Byte &byte);
};


class Pointer {
  const Memory &m;

  // [bid, offset, attributes (1 bit for each)]
  // The top bit of bid is 1 if the block is local, 0 otherwise.
  // A local memory block is a memory block that is
  // allocated by an instruction during the current function call. This does
  // not include allocated blocks from a nested function call. A heap-allocated
  // block can also be a local memory block.
  // Otherwise, a pointer is pointing to a non-local block, which can be either
  // of global variable, heap, or a stackframe that is not this function call.
  // The lowest bits represent whether the pointer value came from a nocapture/
  // readonly argument. If block is local, is-readonly or is-nocapture cannot
  // be 1.
  // TODO: missing support for address space
  smt::expr p;

  smt::expr getValue(const char *name, const smt::FunctionExpr &local_fn,
                      const smt::FunctionExpr &nonlocal_fn,
                      const smt::expr &ret_type, bool src_name = false) const;

public:
  Pointer(const Memory &m, const char *var_name,
          const smt::expr &local = false, bool unique_name = true,
          bool align = true, const smt::expr &attr = smt::expr());
  Pointer(const Memory &m, smt::expr p);
  Pointer(const Memory &m, unsigned bid, bool local);
  Pointer(const Memory &m, const smt::expr &bid, const smt::expr &offset,
          const smt::expr &attrs = smt::expr());

  static unsigned totalBits();
  static unsigned totalBitsShort();

  smt::expr isLocal() const;

  smt::expr getBid() const;
  smt::expr getShortBid() const; // same as getBid but ignoring is_local bit
  smt::expr getOffset() const;
  smt::expr getOffsetSizet() const;
  smt::expr getAttrs() const;
  smt::expr getAddress(bool simplify = true) const;

  smt::expr blockSize() const;

  const smt::expr& operator()() const { return p; }
  // Returns expr with short_bid+offset. It strips attrs away.
  // If this pointer is constructed with var_name (has_attr with false),
  // the returned expr is the variable.
  smt::expr shortPtr() const;
  smt::expr release() { return std::move(p); }
  unsigned bits() const { return p.bits(); }

  Pointer operator+(unsigned) const;
  Pointer operator+(const smt::expr &bytes) const;
  void operator+=(const smt::expr &bytes);

  smt::expr addNoOverflow(const smt::expr &offset) const;

  smt::expr operator==(const Pointer &rhs) const;
  smt::expr operator!=(const Pointer &rhs) const;

  StateValue eq(const Pointer &rhs) const;
  StateValue ne(const Pointer &rhs) const;
  StateValue sle(const Pointer &rhs) const;
  StateValue slt(const Pointer &rhs) const;
  StateValue sge(const Pointer &rhs) const;
  StateValue sgt(const Pointer &rhs) const;
  StateValue ule(const Pointer &rhs) const;
  StateValue ult(const Pointer &rhs) const;
  StateValue uge(const Pointer &rhs) const;
  StateValue ugt(const Pointer &rhs) const;

  smt::expr inbounds(bool simplify_ptr = false, bool strict = false);
  smt::expr blockAlignment() const; // log(bits)
  smt::expr isBlockAligned(unsigned align, bool exact = false) const;
  smt::expr isAligned(unsigned align) const;
  smt::AndExpr isDereferenceable(unsigned bytes, unsigned align, bool iswrite);
  smt::AndExpr isDereferenceable(const smt::expr &bytes, unsigned align,
                                  bool iswrite);
  void isDisjoint(const smt::expr &len1, const Pointer &ptr2,
                   const smt::expr &len2) const;
  smt::expr isBlockAlive() const;
  smt::expr isWritable() const;
  smt::expr isByval() const;

  enum AllocType {
    GLOBAL,
    STACK,
    MALLOC,
    CXX_NEW,
  };
  smt::expr getAllocType() const;
  smt::expr isHeapAllocated() const;
  smt::expr isNocapture() const;
  smt::expr isReadonly() const;
  smt::expr isReadnone() const;

  void stripAttrs();

  smt::expr refined(const Pointer &other) const;
  smt::expr fninputRefined(const Pointer &other, bool is_byval_arg) const;
  smt::expr blockValRefined(const Pointer &other) const;
  smt::expr blockRefined(const Pointer &other) const;

  const Memory& getMemory() const { return m; }

  static Pointer mkNullPointer(const Memory &m);
  smt::expr isNull() const;
  smt::expr isNullBlock() const;
  smt::expr isNonZero() const;

  friend std::ostream& operator<<(std::ostream &os, const Pointer &p);
};


class Memory {
  State *state;

  smt::expr non_local_block_val;  // array: (bid, offset) -> Byte
  smt::expr local_block_val;
  smt::expr initial_non_local_block_val;

  smt::expr non_local_block_liveness; // BV w/ 1 bit per bid (1 if live)
  smt::expr local_block_liveness;

  smt::FunctionExpr local_blk_addr; // bid -> (bits_size_t - 1)
  smt::FunctionExpr local_blk_size;
  smt::FunctionExpr local_blk_align;
  smt::FunctionExpr local_blk_kind;

  std::set<unsigned> non_local_blk_nonwritable;
  smt::FunctionExpr non_local_blk_size;
  smt::FunctionExpr non_local_blk_align;
  smt::FunctionExpr non_local_blk_kind;

  std::vector<unsigned> byval_blks;
  std::vector<bool> escaped_local_blks;

  std::set<smt::expr> undef_vars;

  void store(const Pointer &p, const smt::expr &val, smt::expr &local,
             smt::expr &non_local);

public:
  enum BlockKind {
    MALLOC, CXX_NEW, STACK, GLOBAL, CONSTGLOBAL
  };

  // TODO: missing local_* equivalents
  class CallState {
    smt::expr non_local_block_val;
    smt::expr block_val_var;
    smt::expr non_local_block_liveness;
    smt::expr liveness_var;
    bool empty = true;

  public:
    smt::expr implies(const CallState &st) const;
    friend class Memory;
  };

  Memory(State &state);

  void finishInitialization();
  void mkAxioms(const Memory &other) const;

  static void resetBids(unsigned last_nonlocal);

  void markByVal(unsigned bid);
  smt::expr mkInput(const char *name, const ParamAttrs &attrs) const;
  std::pair<smt::expr, smt::expr> mkUndefInput(const ParamAttrs &attrs) const;

  std::pair<smt::expr, smt::expr>
    mkFnRet(const char *name,
            const std::vector<std::pair<StateValue, bool>> &ptr_inputs) const;
  CallState
    mkCallState(const std::vector<std::pair<StateValue, bool>> *ptr_inputs)
      const;
  void setState(const CallState &st);

  // Allocates a new memory block and returns (pointer expr, allocated).
  // If bid is not specified, it creates a fresh block id by increasing
  // last_bid.
  // If bid is specified, the bid is used, and last_bid is not increased.
  // In this case, it is caller's responsibility to give a unique bid.
  // The newly assigned bid is stored to bid_out if bid_out != nullptr.
  std::pair<smt::expr, smt::expr> alloc(const smt::expr &size, unsigned align,
      BlockKind blockKind, const smt::expr &precond = true,
      const smt::expr &nonnull = false,
      std::optional<unsigned> bid = std::nullopt, unsigned *bid_out = nullptr);

  // Start lifetime of a local block.
  void startLifetime(const smt::expr &ptr_local);

  // If unconstrained is true, the pointer offset, liveness, and block kind
  // are not checked.
  void free(const smt::expr &ptr, bool unconstrained);

  static unsigned getStoreByteSize(const Type &ty);
  void store(const smt::expr &ptr, const StateValue &val, const Type &type,
             unsigned align, const std::set<smt::expr> &undef_vars,
             bool deref_check = true);
  std::pair<StateValue, smt::AndExpr> load(const smt::expr &ptr,
      const Type &type, unsigned align);

  // raw load
  Byte load(const Pointer &p) const;

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytesize, unsigned align,
              const std::set<smt::expr> &undef_vars);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytesize, unsigned align_dst, unsigned align_src,
              bool move);

  smt::expr ptr2int(const smt::expr &ptr) const;
  smt::expr int2ptr(const smt::expr &val) const;

  std::pair<smt::expr,Pointer>
    refined(const Memory &other,
            bool skip_constants,
            const std::vector<std::pair<StateValue, bool>> *set_ptrs = nullptr)
      const;

  // Returns true if a nocapture pointer byte is not in the memory.
  smt::expr checkNocapture() const;

  unsigned numNonlocals() const;

  static Memory mkIf(const smt::expr &cond, const Memory &then,
                     const Memory &els);

  // for container use only
  bool operator<(const Memory &rhs) const;

  void print(std::ostream &os, const smt::Model &m) const;
  friend std::ostream &operator<<(std::ostream &os, const Memory &m);

  friend class Pointer;
};

}
