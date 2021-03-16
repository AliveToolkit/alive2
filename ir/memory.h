#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/pointer.h"
#include "ir/state_value.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include "util/spaceship.h"
#include <compare>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

namespace smt { class Model; }

namespace IR {

class Memory;
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
  Byte(const Memory &m, const StateValue &ptr, unsigned i);

  Byte(const Memory &m, const StateValue &v);

  static Byte mkPoisonByte(const Memory &m);

  smt::expr isPtr() const;
  smt::expr ptrNonpoison() const;
  Pointer ptr() const;
  smt::expr ptrValue() const;
  smt::expr ptrByteoffset() const;
  smt::expr nonptrNonpoison() const;
  smt::expr nonptrValue() const;
  smt::expr isPoison(bool fullbit = true) const;
  smt::expr isZero() const; // zero or null

  smt::expr&& operator()() && { return std::move(p); }

  smt::expr refined(const Byte &other) const;

  smt::expr operator==(const Byte &rhs) const {
    return p == rhs.p;
  }

  bool eq(const Byte &rhs) const {
    return p.eq(rhs.p);
  }

  static unsigned bitsByte();

  friend std::ostream& operator<<(std::ostream &os, const Byte &byte);
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

    smt::expr mayAlias(bool local, const smt::expr &bid) const;

    void setMayAlias(bool local, unsigned bid);
    void setMayAliasUpTo(bool local, unsigned limit); // [0, limit]
    void setNoAlias(bool local, unsigned bid);

    void intersectWith(const AliasSet &other);
    void unionWith(const AliasSet &other);

    void computeAccessStats() const;
    static void printStats(std::ostream &os);

    auto operator<=>(const AliasSet &rhs) const = default;

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

    std::weak_ordering operator<=>(const MemBlock &rhs) const;
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

  bool hasEscapedLocals() const {
    return escaped_local_blks.numMayAlias(true) > 0;
  }

  std::map<smt::expr, AliasSet> ptr_alias; // blockid -> alias
  unsigned next_nonlocal_bid;
  unsigned nextNonlocalBid();

  static bool observesAddresses();
  static int isInitialMemBlock(const smt::expr &e, bool match_any_init = false);

  unsigned numLocals() const;
  unsigned numNonlocals() const;

  smt::expr isBlockAlive(const smt::expr &bid, bool local) const;

  void mk_nonlocal_val_axioms(bool skip_consts);

  bool mayalias(bool local, unsigned bid, const smt::expr &offset,
                unsigned bytes, unsigned align, bool write) const;

  AliasSet computeAliasing(const Pointer &ptr, unsigned btyes, unsigned align,
                           bool write) const;

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
    smt::expr non_local_liveness;
    bool empty = true;

  public:
    static CallState mkIf(const smt::expr &cond, const CallState &then,
                          const CallState &els);
    smt::expr operator==(const CallState &rhs) const;
    auto operator<=>(const CallState &rhs) const = default;
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
    smt::expr operator==(const PtrInput &rhs) const;
    auto operator<=>(const PtrInput &rhs) const = default;
  };

  smt::expr mkFnRet(const char *name, const std::vector<PtrInput> &ptr_inputs);
  CallState mkCallState(const std::string &fnname,
                        const std::vector<PtrInput> *ptr_inputs, bool nofree);
  void setState(const CallState &st);

  // Allocates a new memory block and returns (pointer expr, allocated).
  // If bid is not specified, it creates a fresh block id by increasing
  // last_bid.
  // If bid is specified, the bid is used, and last_bid is not increased.
  // In this case, it is caller's responsibility to give a unique bid.
  // The newly assigned bid is stored to bid_out if bid_out != nullptr.
  // Returns <pointer if allocated, allocated?>
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

  // raw load; NB: no UB check
  Byte load(const Pointer &p, std::set<smt::expr> &undef_vars);

  void memset(const smt::expr &ptr, const StateValue &val,
              const smt::expr &bytesize, unsigned align,
              const std::set<smt::expr> &undef_vars, bool deref_check = true);
  void memcpy(const smt::expr &dst, const smt::expr &src,
              const smt::expr &bytesize, unsigned align_dst, unsigned align_src,
              bool move);

  // full copy of memory blocks
  void copy(const Pointer &src, const Pointer &dst);

  void fillPoison(const smt::expr &bid);

  smt::expr ptr2int(const smt::expr &ptr) const;
  smt::expr int2ptr(const smt::expr &val) const;

  std::tuple<smt::expr, Pointer, std::set<smt::expr>>
    refined(const Memory &other, bool fncall,
            const std::vector<PtrInput> *set_ptrs = nullptr,
            const std::vector<PtrInput> *set_ptrs_other = nullptr) const;

  // Returns true if a nocapture pointer byte is not in the memory.
  smt::expr checkNocapture() const;
  void escapeLocalPtr(const smt::expr &ptr);

  static Memory mkIf(const smt::expr &cond, const Memory &then,
                     const Memory &els);

  auto operator<=>(const Memory &rhs) const = default;

  static void printAliasStats(std::ostream &os) {
    AliasSet::printStats(os);
  }

  void print(std::ostream &os, const smt::Model &m) const;
  friend std::ostream& operator<<(std::ostream &os, const Memory &m);

  friend class Pointer;
};

}
