#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/exprs.h"
#include <ostream>

namespace IR {

class State;
struct StateValue;
class Type;

class ParamAttrs final {
  unsigned bits;

public:
  enum Attribute { None = 0, NonNull = 1<<0, ByVal = 1<<1, NoCapture = 1<<2,
                   NoRead = 1<<3, NoWrite = 1<<4, Dereferenceable = 1<<5,
                   NoUndef = 1<<6, Align = 1<<7, Returned = 1<<8,
                   NoAlias = 1<<9, DereferenceableOrNull = 1<<10 };

  ParamAttrs(unsigned bits = None) : bits(bits) {}

  uint64_t derefBytes = 0;       // Dereferenceable
  uint64_t derefOrNullBytes = 0; // DereferenceableOrNull
  unsigned blockSize = 0;        // exact block size for e.g. byval args
  unsigned align = 1;

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }

  // Returns true if it's UB for the argument to be poison / have a poison elem.
  bool poisonImpliesUB() const
  { return has(Dereferenceable) || has(NoUndef) || has(ByVal) ||
           has(DereferenceableOrNull); }

   // Returns true if it is UB for the argument to be (partially) undef.
  bool undefImpliesUB() const;

  uint64_t getDerefBytes() const;

  friend std::ostream& operator<<(std::ostream &os, const ParamAttrs &attr);

  // Encodes the semantics of attributes using UB and poison.
  std::pair<smt::AndExpr, smt::expr>
      encode(const State &s, const StateValue &val, const Type &ty) const;
};


class FnAttrs final {
  unsigned bits;

public:
  enum Attribute { None = 0, NoRead = 1 << 0, NoWrite = 1 << 1,
                   ArgMemOnly = 1 << 2, NNaN = 1 << 3, NoReturn = 1 << 4,
                   Dereferenceable = 1 << 5, NonNull = 1 << 6,
                   NoFree = 1 << 7, NoUndef = 1 << 8, Align = 1 << 9,
                   NoThrow = 1 << 10, NoAlias = 1 << 11, WillReturn = 1 << 12,
                   DereferenceableOrNull = 1 << 13 };

  FnAttrs(unsigned bits = None) : bits(bits) {}

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }

  uint64_t derefBytes = 0;       // Dereferenceable
  uint64_t derefOrNullBytes = 0; // DereferenceableOrNull
  unsigned align = 1;

  // Returns true if returning poison or an aggregate having a poison is UB
  bool poisonImpliesUB() const;

  // Returns true if returning (partially) undef is UB
  bool undefImpliesUB() const;

  friend std::ostream& operator<<(std::ostream &os, const FnAttrs &attr);

  // Encodes the semantics of attributes using UB and poison.
  std::pair<smt::AndExpr, smt::expr>
      encode(const State &s, const StateValue &val, const Type &ty) const;
};

}
