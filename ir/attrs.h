#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/functions.h"
#include "smt/exprs.h"
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace IR {

class Instr;
class ParamAttrs;
class State;
struct StateValue;
class Type;
class Value;

class MemoryAccess final {
  unsigned val = 0;
  MemoryAccess(unsigned val) : val(val) {}

public:
  enum AccessType { Args, Globals, Inaccessible, Errno, Other, NumTypes };
  MemoryAccess() = default;

  bool canRead(AccessType ty) const;
  bool canWrite(AccessType ty) const;
  bool canOnlyRead(AccessType ty) const;
  bool canOnlyWrite(AccessType ty) const;
  bool canAccessAnything() const;
  bool canReadAnything() const;
  bool canWriteAnything() const;
  bool canReadSomething() const;
  bool canWriteSomething() const;

  void setNoAccess() { val = 0; }
  void setFullAccess();
  void setCanOnlyRead();
  void setCanOnlyWrite();
  void setCanOnlyRead(AccessType ty);
  void setCanOnlyWrite(AccessType ty);
  void setCanOnlyAccess(AccessType ty);

  void setCanAlsoRead(AccessType ty);
  void setCanAlsoWrite(AccessType ty);
  void setCanAlsoAccess(AccessType ty);

  void operator&=(MemoryAccess rhs) { val &= rhs.val; }
  void operator|=(MemoryAccess rhs) { val |= rhs.val; }
  MemoryAccess operator|(MemoryAccess rhs) const { return val | rhs.val; }

  auto operator<=>(const MemoryAccess &rhs) const = default;
  friend std::ostream& operator<<(std::ostream &os, const MemoryAccess &a);
  friend class SMTMemoryAccess;
};


class ParamAttrs final {
  unsigned bits;

public:
  enum Attribute { None = 0, NonNull = 1<<0, ByVal = 1<<1, NoCapture = 1<<2,
                   NoRead = 1<<3, NoWrite = 1<<4, Dereferenceable = 1<<5,
                   NoUndef = 1<<6, Align = 1<<7, Returned = 1<<8,
                   NoAlias = 1<<9, DereferenceableOrNull = 1<<10,
                   AllocPtr = 1<<11, AllocAlign = 1<<12,
                   ZeroExt = 1<<13, SignExt = 1<<14, InReg = 1<<15,
                   NoFPClass = 1<<16, DeadOnUnwind = 1<<17,
                   Writable = 1<<18, DeadOnReturn = 1<<19,
                   IsArg = 1<<31 // used internally to make values as arguments
                  };

  ParamAttrs(unsigned bits = None) : bits(bits) {}

  uint64_t derefBytes = 0;       // Dereferenceable
  uint64_t derefOrNullBytes = 0; // DereferenceableOrNull
  uint64_t blockSize = 0;        // exact block size for e.g. byval args
  uint64_t align = 1;
  uint16_t nofpclass = 0;

  std::vector<std::pair<uint64_t, uint64_t>> initializes;

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }
  bool refinedBy(const ParamAttrs &other) const;

  // Returns true if it's UB for the argument to be poison / have a poison elem.
  bool poisonImpliesUB() const;

  uint64_t getDerefBytes() const;
  uint64_t maxAccessSize() const;

  void merge(const ParamAttrs &other);

  friend std::ostream& operator<<(std::ostream &os, const ParamAttrs &attr);

  // Encodes the semantics of attributes using UB and poison.
  StateValue encode(State &s, StateValue &&val, const Type &ty,
                    bool isdecl = false) const;
};

struct FPDenormalAttrs {
  enum Type { IEEE, PositiveZero, PreserveSign, Dynamic };
  Type input = IEEE;
  Type output = IEEE;

  void print(std::ostream &os, bool is_fp32 = false) const;
  auto operator<=>(const FPDenormalAttrs &rhs) const = default;
};

enum class AllocKind {
  Alloc         = 1 << 0,
  Realloc       = 1 << 1,
  Free          = 1 << 2,
  Uninitialized = 1 << 3,
  Zeroed        = 1 << 4,
  Aligned       = 1 << 5
};

class FnAttrs final {
  FPDenormalAttrs fp_denormal;
  std::optional<FPDenormalAttrs> fp_denormal32;
  unsigned bits;
  uint8_t allockind = 0;

public:
  enum Attribute { None = 0, NNaN = 1 << 0, NoReturn = 1 << 1,
                   Dereferenceable = 1 << 2, NonNull = 1 << 3,
                   NoFree = 1 << 4, NoUndef = 1 << 5, Align = 1 << 6,
                   NoThrow = 1 << 7, NoAlias = 1 << 8, WillReturn = 1 << 9,
                   DereferenceableOrNull = 1 << 10,
                   NullPointerIsValid = 1 << 11,
                   AllocSize = 1 << 12, ZeroExt = 1<<13,
                   SignExt = 1<<14, NoFPClass = 1<<15, Asm = 1<<16 };

  FnAttrs(unsigned bits = None) : bits(bits) {}

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }

  uint64_t derefBytes = 0;       // Dereferenceable
  uint64_t derefOrNullBytes = 0; // DereferenceableOrNull
  uint64_t align = 1;

  unsigned allocsize_0 = 0;      // AllocSize
  unsigned allocsize_1 = -1u;    // AllocSize

  MemoryAccess mem;

  std::string allocfamily;

  uint16_t nofpclass = 0;

  void add(AllocKind k) { allockind |= (uint8_t)k; }
  bool has(AllocKind k) const { return allockind & (uint8_t)k; }
  bool isAlloc() const { return allockind != 0 || has(AllocSize); }

  void inferImpliedAttributes();

  std::pair<smt::expr,smt::expr>
  computeAllocSize(State &s,
                   const std::vector<std::pair<Value*, ParamAttrs>> &args) const;

  bool isNonNull() const;

  // Returns true if returning poison or an aggregate having a poison is UB
  bool poisonImpliesUB() const;

  void setFPDenormal(FPDenormalAttrs attr, unsigned bits = 0);
  FPDenormalAttrs getFPDenormal(const Type &ty) const;

  bool refinedBy(const FnAttrs &other) const;

  // Encodes the semantics of attributes using UB and poison.
  StateValue encode(State &s, StateValue &&val, const Type &ty,
                    const smt::expr &allocsize,
                    Value *allocalign) const;

  friend std::ostream& operator<<(std::ostream &os, const FnAttrs &attr);
};


struct FastMathFlags final {
  enum Flags {
    None = 0, NNaN = 1 << 0, NInf = 1 << 1, NSZ = 1 << 2, ARCP = 1 << 3,
    Contract = 1 << 4, Reassoc = 1 << 5, AFN = 1 << 6,
    FastMath = NNaN | NInf | NSZ | ARCP | Contract | Reassoc | AFN
  };
  unsigned flags = None;

  bool isNone() const { return flags == None; }
  friend std::ostream& operator<<(std::ostream &os, const FastMathFlags &fm);
};


struct FpRoundingMode final {
  enum Mode { RNE, RNA, RTP, RTN, RTZ, Dynamic, Default } mode;
  FpRoundingMode() : mode(Default) {}
  FpRoundingMode(Mode mode) : mode(mode) {}
  bool isDynamic() const { return mode == Dynamic; }
  bool isDefault() const { return mode == Default; }
  Mode getMode() const { return mode; }
  smt::expr toSMT() const;
  friend std::ostream& operator<<(std::ostream &os, FpRoundingMode rounding);
};


struct FpExceptionMode final {
  enum Mode { Ignore, MayTrap, Strict } mode;
  FpExceptionMode() : mode(Ignore) {}
  FpExceptionMode(Mode mode) : mode(mode) {}
  Mode getMode() const { return mode; }
  bool ignore() const { return mode == Ignore; }
  friend std::ostream& operator<<(std::ostream &os, FpExceptionMode ex);
};

smt::expr isfpclass(const smt::expr &v, const Type &ty, uint16_t mask);


struct TailCallInfo final {
  enum TailCallType { None, Tail, MustTail } type = None;
  // Determine if callee and caller have the same calling convention.
  bool has_same_calling_convention = true;

  void check(State &s, const Instr &i, const std::vector<PtrInput> &args) const;
  friend std::ostream& operator<<(std::ostream &os, const TailCallInfo &tci);
};

}
