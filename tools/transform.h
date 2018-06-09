#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/solver.h"
#include "util/errors.h"
#include <string>
#include <ostream>

namespace tools {

struct TransformPrintOpts {
  bool print_fn_header = true;
};


class TypingAssignments {
  smt::Solver s;
  smt::Result r;
  TypingAssignments(const smt::expr &e);

public:
  bool operator!() const { return !r.isSat(); }
  operator bool() const { return r.isSat(); }
  void operator++(void);

  friend class Transform;
};


struct Transform {
  std::string name;
  IR::Function src, tgt;

  util::Errors verify() const;
  TypingAssignments getTypings() const;
  void fixupTypes(const TypingAssignments &t);

  void print(std::ostream &os, const TransformPrintOpts &opt) const;
  friend std::ostream& operator<<(std::ostream &os, const Transform &t);
};

}
