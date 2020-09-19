#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>

namespace IR {

class ParamAttrs final {
  unsigned bits;

public:
  enum Attribute { None = 0, NonNull = 1<<0, ByVal = 1<<1, NoCapture = 1<<2,
                   ReadOnly = 1<<3, ReadNone = 1<<4, Dereferenceable = 1<<5,
                   NoUndef = 1<<6, Align = 1<<7 };

  ParamAttrs(unsigned bits = None) : bits(bits) {}

  uint64_t derefBytes; // Dereferenceable
  uint64_t blockSize;  // exact block size for e.g. byval args
  uint64_t align;      // power-of-2 in bytes

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }

  // Returns true if it is UB for the argument to be poison / have poison elems.
  bool poisonImpliesUB() const
  { return has(NonNull) || has(Dereferenceable) || has(NoUndef) || has(ByVal); }

   // Returns true if it is UB for the argument to be (partially) undef.
  bool undefImpliesUB() const
  { return has(NoUndef); }

  friend std::ostream& operator<<(std::ostream &os, const ParamAttrs &attr);
};


class FnAttrs final {
  unsigned bits;
  uint64_t derefBytes;

public:
  enum Attribute { None = 0, NoRead = 1 << 0, NoWrite = 1 << 1,
                   ArgMemOnly = 1 << 2, NNaN = 1 << 3, NoReturn = 1 << 4,
                   Dereferenceable = 1 << 5, NonNull = 1 << 6,
                   NoFree = 1 << 7, NoUndef = 1 << 8 };

  FnAttrs(unsigned bits = None) : bits(bits) {}

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }
  uint64_t getDerefBytes() const { return derefBytes; }
  void setDerefBytes(uint64_t bytes) { derefBytes = bytes; }

  // Returns true if returning poison is UB
  bool poisonImpliesUB() const
  { return has(NonNull) || has(Dereferenceable) || has(NoUndef); }

  // Returns true if returning (partially) undef is UB
  bool undefImpliesUB() const
  { return has(NoUndef); }

  friend std::ostream& operator<<(std::ostream &os, const FnAttrs &attr);
};

}
