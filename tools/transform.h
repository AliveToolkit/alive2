#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/solver.h"
#include "util/errors.h"
#include <string>
#include <ostream>
#include <unordered_map>

namespace tools {

struct TransformPrintOpts {
  bool print_fn_header = true;
};


struct Transform {
  std::string name;
  IR::Function src, tgt;
  IR::Predicate *precondition = nullptr;

  void print(std::ostream &os, const TransformPrintOpts &opt) const;
  friend std::ostream& operator<<(std::ostream &os, const Transform &t);
};


class TypingAssignments {
  smt::Solver s, sneg;
  smt::Result r;
  bool has_only_one_solution = false;
  bool is_unsat = false;
  TypingAssignments(const smt::expr &e);

public:
  bool operator!() const { return !(bool)*this; }
  operator bool() const;
  void operator++(void);
  bool hasSingleTyping() const { return has_only_one_solution; }

  friend class TransformVerify;
};


class TransformVerify {
  Transform &t;
  std::unordered_map<std::string, const IR::Instr*> tgt_instrs;
  bool check_each_var;

public:
  TransformVerify(Transform &t, bool check_each_var);
  util::Errors sync();
  util::Errors verify() const;
  TypingAssignments getTypings() const;
  void fixupTypes(const TypingAssignments &ty);
};

}
