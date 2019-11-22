#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state_value.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <array>
#include <ostream>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace IR {

// The upperbound of # of non-local pointers that are returned by instructions.
extern unsigned num_max_nonlocals_inst;
// The upperbound of # of local blocks (max(src, tgt))
extern unsigned num_locals;
// The upperbound of # of nonlocal blocks
// It is num_globals + num_ptrinputs + num_max_nonlocals_inst + 1(null block).
extern unsigned num_nonlocals;
// The number of bits needed for encoding block id.
extern unsigned bits_for_bid;
// The number of bits needed for encoding pointer's offset.
extern unsigned bits_for_offset;

void initConstants(unsigned num_globals, unsigned num_ptrinputs,
                   unsigned num_max_nonlocals_inst, unsigned num_locals);


class Value;
class BasicBlock;
class Function;

class State {
public:
  using ValTy = std::pair<StateValue, std::set<smt::expr>>;
  // DomainTy: (reachability from entry to this basic block, undef vars)
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

  // Global variables' memory block ids
  std::unordered_map<std::string, unsigned> glbvar_bids;

  // temp state
  DomainTy domain;
  Memory memory;
  std::set<smt::expr> undef_vars;
  std::array<StateValue, 32> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  // return_domain: a boolean expression describing return condition
  smt::expr return_domain;
  smt::DisjointExpr<StateValue> return_val;
  smt::DisjointExpr<Memory> return_memory;
  std::set<smt::expr> return_undef_vars;

public:
  State(const Function &f, bool source);

  static void resetGlobals();

  const StateValue& exec(const Value &v);
  const StateValue& operator[](const Value &val);
  const ValTy& at(const Value &val) const;
  const smt::expr* jumpCondFrom(const BasicBlock &bb) const;

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
  void addUndefVar(smt::expr &&var);
  void resetUndefVars();

  auto& getFn() const { return f; }
  auto& getMemory() { return memory; }
  auto& getAxioms() const { return axioms; }
  auto& getPre() const { return precondition; }
  const auto& getValues() const { return values; }
  const auto& getQuantVars() const { return quantified_vars; }

  auto& returnDomain() const { return return_domain; }
  Memory returnMemory() const { return return_memory(); }

  std::pair<StateValue, const std::set<smt::expr>&> returnVal() const {
    return { return_val(), return_undef_vars };
  }

  void startParsingPre() { disable_undef_rewrite = true; }

  // whether this is source or target program
  bool isSource() const { return source; }

  void addGlobalVarBid(const std::string &glbvar, unsigned bid);
  // Checks whether glbvar has block id assigned.
  // If it has, bid is updated with the block id.
  bool hasGlobalVarBid(const std::string &glbvar, unsigned &bid) const;
  void copyGlobalVarBidsFromSrc(const State &src);

private:
  void addJump(const BasicBlock &dst, smt::expr &&domain);
};

}
