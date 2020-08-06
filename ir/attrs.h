#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>

namespace IR {

class ParamAttrs final {
  unsigned bits;
  uint64_t derefBytes;

public:
  enum Attribute { None = 0, NonNull = 1<<0, ByVal = 1<<1, NoCapture = 1<<2,
                   ReadOnly = 1<<3, ReadNone = 1<<4, Dereferenceable = 1<<5,
                   NoUndef = 1<<6 };

  ParamAttrs(unsigned bits = None) : bits(bits) {}

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits |= (unsigned)a; }
  uint64_t getDerefBytes() const { return derefBytes; }
  void setDerefBytes(uint64_t bytes) { derefBytes = bytes; }

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

  friend std::ostream& operator<<(std::ostream &os, const FnAttrs &attr);
};

}
