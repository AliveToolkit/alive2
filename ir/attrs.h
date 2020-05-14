#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>

namespace IR {

class ParamAttrs final {
public:
  enum Attribute { None = 0, NonNull = 1<<0, ByVal = 1<<1, NoCapture = 1<<2,
                   ReadOnly = 1<<3, ReadNone = 1<<4 };

  unsigned bits;

  ParamAttrs(unsigned bits = None) : bits(bits) {}

  std::string str() const {
    std::string ret;
    if (has(ParamAttrs::NonNull))
      ret += "nonnull ";
    if (has(ParamAttrs::ByVal))
      ret += "byval ";
    if (has(ParamAttrs::NoCapture))
      ret += "nocapture ";
    if (has(ParamAttrs::ReadOnly))
      ret += "readonly ";
    if (has(ParamAttrs::ReadNone))
      ret += "readnone ";
    return ret;
  }

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits = bits | (unsigned)a; }
};


class FnAttrs final {
public:
  enum Attribute { None = 0, NoRead = 1 << 0, NoWrite = 1 << 1,
                   ArgMemOnly = 1 << 2, NNaN = 1 << 3, NoReturn = 1 << 4 };

  unsigned bits;

  FnAttrs(unsigned bits = None) : bits(bits) {}

  std::string str() const {
    std::string ret;
    if (bits & NoRead)
      ret += " noread";
    if (bits & NoWrite)
      ret += " nowrite";
    if (bits & ArgMemOnly)
      ret += " argmemonly";
    if (bits & NNaN)
      ret += " NNaN";
    if (bits & NoReturn)
      ret += " noreturn";
    return ret;
  }

  bool has(Attribute a) const { return (bits & a) != 0; }
  void set(Attribute a) { bits = bits | (unsigned)a; }
};

}
