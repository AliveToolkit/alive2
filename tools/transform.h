#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/solver.h"
#include "util/errors.h"
#include <set>
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

  void print(std::ostream &os, const TransformPrintOpts &opt) const;
  friend std::ostream& operator<<(std::ostream &os, const Transform &t);
};


class TypingAssignments {
  smt::Solver s;
  smt::Result r;
  bool has_only_one_solution = false;
  bool is_unsat = false;
  TypingAssignments(const smt::expr &e);

public:
  bool operator!() const { return !(bool)*this; }
  operator bool() const;
  void operator++(void);

  friend class TransformVerify;
};


class TransformVerify {
  Transform &t;
  std::unordered_map<std::string, const IR::Instr*> tgt_instrs;
  bool check_each_var;

public:
  TransformVerify(Transform &t, bool check_each_var);
  util::Errors verify() const;
  TypingAssignments getTypings() const;
  void fixupTypes(const TypingAssignments &ty);
};

smt::expr preprocess(Transform &t, const std::set<smt::expr> &qvars,
                       const std::set<smt::expr> &undef_qvars, smt::expr && e);

void error(util::Errors &errs, IR::State &src_state, IR::State &tgt_state,
                  const smt::Result &r, bool print_var, const IR::Value *var,
                  const IR::StateValue &src, const IR::StateValue &tgt,
                  const char *msg, bool check_each_var);

}
