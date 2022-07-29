#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace IR {

class ParamAttrs;
class State;
struct StateValue;
class Type;
class Value;

class ParamAttrs final {
  unsigned bits;

public:
  enum Attribute { None = 0, NonNull = 1<<0, ByVal = 1<<1, NoCapture = 1<<2,
                   NoRead = 1<<3, NoWrite = 1<<4, Dereferenceable = 1<<5,
                   NoUndef = 1<<6, Align = 1<<7, Returned = 1<<8,
                   NoAlias = 1<<9, DereferenceableOrNull = 1<<10,
                   AllocPtr = 1<<11, AllocAlign = 1<<12 };

  ParamAttrs(unsigned bits = None) : bits(bits) {}

  uint64_t derefBytes = 0;       // Dereferenceable
  uint64_t derefOrNullBytes = 0; // DereferenceableOrNull
  uint64_t blockSize = 0;        // exact block size for e.g. byval args
  uint64_t align = 1;

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }
  bool refinedBy(const ParamAttrs &other) const;

  // Returns true if it's UB for the argument to be poison / have a poison elem.
  bool poisonImpliesUB() const;

  // Returns true if it is UB for the argument to be (partially) undef.
  bool undefImpliesUB() const;

  uint64_t getDerefBytes() const;

  void merge(const ParamAttrs &other);

  friend std::ostream& operator<<(std::ostream &os, const ParamAttrs &attr);

  // Encodes the semantics of attributes using UB and poison.
  StateValue encode(State &s, StateValue &&val, const Type &ty) const;
};

struct FPDenormalAttrs {
  enum Type { IEEE, PreserveSign, PositiveZero };
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
  enum Attribute { None = 0, NoRead = 1 << 0, NoWrite = 1 << 1,
                   ArgMemOnly = 1 << 2, NNaN = 1 << 3, NoReturn = 1 << 4,
                   Dereferenceable = 1 << 5, NonNull = 1 << 6,
                   NoFree = 1 << 7, NoUndef = 1 << 8, Align = 1 << 9,
                   NoThrow = 1 << 10, NoAlias = 1 << 11, WillReturn = 1 << 12,
                   DereferenceableOrNull = 1 << 13,
                   InaccessibleMemOnly = 1 << 14,
                   NullPointerIsValid = 1 << 15,
                   AllocSize = 1 << 16 };

  FnAttrs(unsigned bits = None) : bits(bits) {}

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }

  uint64_t derefBytes = 0;       // Dereferenceable
  uint64_t derefOrNullBytes = 0; // DereferenceableOrNull
  uint64_t align = 0;

  unsigned allocsize_0;
  unsigned allocsize_1 = -1u;

  std::string allocfamily;

  void add(AllocKind k) { allockind |= (uint8_t)k; }
  bool has(AllocKind k) const { return allockind & (uint8_t)k; }
  bool isAlloc() const { return allockind != 0; }

  std::pair<smt::expr,smt::expr>
  computeAllocSize(State &s,
                   const std::vector<std::pair<Value*, ParamAttrs>> &args) const;

  bool isNonNull() const;

  // Returns true if returning poison or an aggregate having a poison is UB
  bool poisonImpliesUB() const;

  // Returns true if returning (partially) undef is UB
  bool undefImpliesUB() const;

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
  friend std::ostream& operator<<(std::ostream &os, FpExceptionMode ex);
};

}
