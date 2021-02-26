#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include "ir/state_value.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <ostream>
#include <set>

namespace IR {

class Memory;

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
  Pointer(const Memory &m, const smt::expr &bid, const smt::expr &offset,
          const smt::expr &attr);
  Pointer(const Memory &m, const char *var_name,
          const smt::expr &local = false, bool unique_name = true,
          bool align = true, const ParamAttrs &attr = {});
  Pointer(const Memory &m, smt::expr p);
  Pointer(const Memory &m, unsigned bid, bool local);
  Pointer(const Memory &m, const smt::expr &bid, const smt::expr &offset,
          const ParamAttrs &attr = {});

  static smt::expr mkLongBid(const smt::expr &short_bid, bool local);

  static unsigned totalBits();
  static unsigned bitsShortBid();
  static unsigned bitsShortOffset();
  static unsigned zeroBitsShortOffset();

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
  smt::AndExpr isDereferenceable(uint64_t bytes, unsigned align,
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
  smt::expr isStackAllocated() const;
  smt::expr isHeapAllocated() const;
  smt::expr isNocapture(bool simplify = true) const;
  smt::expr isNoRead() const;
  smt::expr isNoWrite() const;

  smt::expr refined(const Pointer &other) const;
  smt::expr fninputRefined(const Pointer &other, std::set<smt::expr> &undef,
                           bool is_byval_arg) const;

  const Memory& getMemory() const { return m; }

  static Pointer mkNullPointer(const Memory &m);
  smt::expr isNull() const;
  smt::expr isNonZero() const;

  static void resetGlobals();

  auto operator<=>(const Pointer &rhs) const { return p <=> rhs.p; }

  friend std::ostream& operator<<(std::ostream &os, const Pointer &p);
};

}
