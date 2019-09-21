#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state_value.h"
#include "smt/expr.h"
#include <array>
#include <optional>
#include <ostream>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace IR {

struct LoopInCFGDetected : public std::exception {};
struct OutOfMemory : public std::exception {};

class Value;
class BasicBlock;
class Function;

class State {
public:
  using ValTy = std::pair<StateValue, std::set<smt::expr>>;
  using DomainTy = std::pair<smt::expr, std::set<smt::expr>>;

private:
  const Function &f;
  bool source;
  bool disable_undef_rewrite = false;
  smt::expr precondition = true;
  smt::expr axioms = true;

  const BasicBlock *current_bb;
  std::set<smt::expr> quantified_vars;

  // var -> ((value, not_poison), undef_vars, already_used?)
  std::unordered_map<const Value*, unsigned> values_map;
  std::vector<std::tuple<const Value*, ValTy, bool>> values;

  // dst BB -> src BB -> (domain data, memory)
  std::unordered_map<const BasicBlock*,
                     std::unordered_map<const BasicBlock*,
                                        std::pair<DomainTy, Memory>>>
    predecessor_data;
  std::unordered_set<const BasicBlock*> seen_bbs;

  // The initial memory at the first basic block.
  // startBB() initializes this variable when starting entry.
  std::optional<Memory> memoryAtEntry;

  // temp state
  DomainTy domain;
  Memory memory;
  std::set<smt::expr> undef_vars;
  std::array<StateValue, 32> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  smt::expr return_domain;
  // FIXME: replace with disjoint expr builder
  ValTy return_val;
  bool returned = false;

public:
  State(const Function &f, bool source);

  const StateValue& exec(const Value &v);
  const StateValue& operator[](const Value &val);
  const ValTy& at(const Value &val) const;
  const smt::expr* jumpCondFrom(const BasicBlock &bb) const;

  // Returns the initial memory at the first basic block.
  const Memory &getMemoryAtEntry() const;

  bool startBB(const BasicBlock &bb);
  void addJump(const BasicBlock &dst);
  // boolean cond
  void addJump(StateValue &&cond, const BasicBlock &dst);
  // i1 cond
  void addCondJump(const StateValue &cond, const BasicBlock &dst_true,
                   const BasicBlock &dst_false);
  void addReturn(const StateValue &val);

  void addAxiom(smt::expr &&axiom) { axioms &= std::move(axiom); }
  void addPre(smt::expr &&cond) { precondition &= std::move(cond); }
  void addUB(smt::expr &&ub);
  void addUB(const smt::expr &ub);

  void addQuantVar(const smt::expr &var);
  void addUndefVar(const smt::expr &var);
  void resetUndefVars();

  auto& getFn() const { return f; }
  auto& getMemory() { return memory; }
  auto& getAxioms() const { return axioms; }
  auto& getPre() const { return precondition; }
  const auto& getValues() const { return values; }
  const auto& getQuantVars() const { return quantified_vars; }

  bool fnReturned() const { return returned; }
  auto& returnDomain() const { return return_domain; }
  auto& returnVal() const { return return_val; }

  void startParsingPre() { disable_undef_rewrite = true; }

  // whether this is source or target program
  bool isSource() const { return source; }

private:
  void addJump(const BasicBlock &dst, smt::expr &&domain);
};

}
