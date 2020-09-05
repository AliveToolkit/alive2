#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/globals.h"
#include "ir/state_value.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <map>
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

  bool eq(const Byte &rhs) const {
    return p.eq(rhs.p);
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
  Pointer(const Memory &m, unsigned bid, bool local, const smt::expr &offset);
  Pointer(const Memory &m, const smt::expr &bid, const smt::expr &offset,
          const smt::expr &attrs = smt::expr());

  static unsigned totalBits();
  static unsigned totalBitsShort();
  static unsigned bitsShortOffset();

  smt::expr isLocal(bool simplify = true) const;

  smt::expr getBid() const;
  smt::expr getShortBid() const; // same as getBid but ignoring is_local bit
  smt::expr getOffset() const;
  smt::expr getOffsetSizet() const;
  smt::expr getShortOffset() const; // same as getOffset but skips aligned bits
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
  smt::expr isAligned(unsigned align);
  smt::AndExpr isDereferenceable(uint64_t bytes, unsigned align = bits_byte / 8,
                                 bool iswrite = false);
  smt::AndExpr isDereferenceable(const smt::expr &bytes, unsigned align,
                                 bool iswrite);
  void isDisjointOrEqual(const smt::expr &len1, const Pointer &ptr2,
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
  smt::expr isNocapture(bool simplify = true) const;
  smt::expr isReadonly() const;
  smt::expr isReadnone() const;

  void stripAttrs();

  smt::expr refined(const Pointer &other) const;
  smt::expr fninputRefined(const Pointer &other, std::set<smt::expr> &undef,
                           bool is_byval_arg) const;

  const Memory& getMemory() const { return m; }

  static Pointer mkNullPointer(const Memory &m);
  smt::expr isNull() const;
  smt::expr isNonZero() const;

  // for container use only
  bool operator<(const Pointer &rhs) const;

  friend std::ostream& operator<<(std::ostream &os, const Pointer &p);
};


class Memory {
  State *state;

  class AliasSet {
    std::vector<bool> local, non_local;

  public:
    AliasSet(const Memory &m); // no alias
    size_t size(bool local) const;

    int isFullUpToAlias(bool local) const; // >= 0 if up to
    bool mayAlias(bool local, unsigned bid) const;
    unsigned numMayAlias(bool local) const;

    void setMayAlias(bool local, unsigned bid);
    void setMayAliasUpTo(bool local, unsigned limit); // [0, limit]
    void setNoAlias(bool local, unsigned bid);

    void intersectWith(const AliasSet &other);
    void unionWith(const AliasSet &other);

    void computeAccessStats() const;
    static void printStats(std::ostream &os);

    // for container use only
    bool operator<(const AliasSet &rhs) const;

    void print(std::ostream &os) const;
  };

  enum DataType { DATA_NONE = 0, DATA_INT = 1, DATA_PTR = 2,
                  DATA_ANY = DATA_INT | DATA_PTR };

  struct MemBlock {
    smt::expr val; // array: short offset -> Byte
    std::set<smt::expr> undef;
    unsigned char type = DATA_ANY;

    MemBlock() {}
    MemBlock(smt::expr &&val) : val(std::move(val)) {}
    MemBlock(smt::expr &&val, DataType type)
      : val(std::move(val)), type(type) {}

    bool operator<(const MemBlock &other) const {
      return std::tie(val, undef, type) <
             std::tie(other.val, other.undef, other.type);
    }
  };

  std::vector<MemBlock> non_local_block_val;
  std::vector<MemBlock> local_block_val;

  smt::expr non_local_block_liveness; // BV w/ 1 bit per bid (1 if live)
  smt::expr local_block_liveness;

  smt::FunctionExpr local_blk_addr; // bid -> (bits_size_t - 1)
  smt::FunctionExpr local_blk_size;
  smt::FunctionExpr local_blk_align;
  smt::FunctionExpr local_blk_kind;

  smt::FunctionExpr non_local_blk_size;
  smt::FunctionExpr non_local_blk_align;
  smt::FunctionExpr non_local_blk_kind;

  std::vector<unsigned> byval_blks;
  AliasSet escaped_local_blks;

  std::map<smt::expr, AliasSet> ptr_alias; // blockid -> alias
  unsigned next_nonlocal_bid;
  unsigned nextNonlocalBid();

  unsigned numLocals() const;
  unsigned numNonlocals() const;

  void mk_nonlocal_val_axioms(bool skip_consts);

  bool mayalias(bool local, unsigned bid, const smt::expr &offset,
                unsigned bytes, unsigned align, bool write) const;

  template <typename Fn>
  void access(const Pointer &ptr, unsigned btyes, unsigned align, bool write,
              Fn &fn);

  std::vector<Byte> load(const Pointer &ptr, unsigned bytes,
                         std::set<smt::expr> &undef, unsigned align,
                         bool left2right = true,
                         DataType type = DATA_ANY);
  StateValue load(const Pointer &ptr, const Type &type,
                  std::set<smt::expr> &undef, unsigned align);

  DataType data_type(const std::vector<std::pair<unsigned, smt::expr>> &data,
                     bool full_store) const;

  void store(const Pointer &ptr,
             const std::vector<std::pair<unsigned, smt::expr>> &data,
             const std::set<smt::expr> &undef, unsigned align);
  void store(const StateValue &val, const Type &type, unsigned offset,
             std::vector<std::pair<unsigned, smt::expr>> &data);

  void storeLambda(const Pointer &ptr, const smt::expr &offset,
                   const smt::expr &bytes, const smt::expr &val,
                   const std::set<smt::expr> &undef, unsigned align);

  smt::expr blockValRefined(const Memory &other, unsigned bid, bool local,
                            const smt::expr &offset,
                            std::set<smt::expr> &undef) const;
  smt::expr blockRefined(const Pointer &src, const Pointer &tgt, unsigned bid,
                         std::set<smt::expr> &undef) const;

public:
  enum BlockKind {
    MALLOC, CXX_NEW, STACK, GLOBAL, CONSTGLOBAL
  };

  // TODO: missing local_* equivalents
  class CallState {
    std::vector<smt::expr> non_local_block_val;
    smt::expr non_local_block_liveness;
    smt::expr liveness_var;
    bool empty = true;

  public:
    smt::expr implies(const CallState &st) const;
    friend class Memory;
  };

  Memory(State &state);

  void mkAxioms(const Memory &other) const;

  static void resetGlobals();
  void syncWithSrc(const Memory &src);

  void markByVal(unsigned bid);
  smt::expr mkInput(const char *name, const ParamAttrs &attrs);
  std::pair<smt::expr, smt::expr> mkUndefInput(const ParamAttrs &attrs) const;

  struct PtrInput {
    StateValue val;
    bool byval;
    bool nocapture;

    PtrInput(StateValue &&v, bool byval, bool nocapture) :
      val(std::move(v)), byval(byval), nocapture(nocapture) {}
    bool operator<(const PtrInput &rhs) const {
      return std::tie(val, byval, nocapture) <
             std::tie(rhs.val, rhs.byval, rhs.nocapture);
    }
  };

  std::pair<smt::expr, smt::expr>
    mkFnRet(const char *name, const std::vector<PtrInput> &ptr_inputs);
  CallState mkCallState(const std::vector<PtrInput> *ptr_inputs, bool nofree)
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
             unsigned align, const std::set<smt::expr> &undef_vars);
  std::pair<StateValue, smt::AndExpr>
    load(const smt::expr &ptr, const Type &type, unsigned align);

  // raw load
  Byte load(const Pointer &p, std::set<smt::expr> &undef_vars, unsigned align);

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytesize, unsigned align,
              const std::set<smt::expr> &undef_vars, bool deref_check = true);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytesize, unsigned align_dst, unsigned align_src,
              bool move);

  // full copy of memory blocks
  void copy(const Pointer &src, const Pointer &dst);

  smt::expr ptr2int(const smt::expr &ptr) const;
  smt::expr int2ptr(const smt::expr &val) const;

  std::tuple<smt::expr, Pointer, std::set<smt::expr>>
    refined(const Memory &other,
            bool skip_constants,
            const std::vector<PtrInput> *set_ptrs = nullptr)
      const;

  // Returns true if a nocapture pointer byte is not in the memory.
  smt::expr checkNocapture() const;
  void escapeLocalPtr(const smt::expr &ptr);

  static Memory mkIf(const smt::expr &cond, const Memory &then,
                     const Memory &els);

  // for container use only
  bool operator<(const Memory &rhs) const;

  static void printAliasStats(std::ostream &os) {
    AliasSet::printStats(os);
  }

  void print(std::ostream &os, const smt::Model &m) const;
  friend std::ostream& operator<<(std::ostream &os, const Memory &m);

  friend class Pointer;
};

}
