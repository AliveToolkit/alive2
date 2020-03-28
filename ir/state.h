#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/memory.h"
#include "ir/state_value.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <array>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace IR {

class Value;
class BasicBlock;
class Function;

class State {
public:
  using ValTy = std::pair<StateValue, std::set<smt::expr>>;

private:
  struct CurrentDomain {
    smt::expr path; // path from fn entry
    smt::AndExpr UB;
    std::set<smt::expr> undef_vars;

    smt::expr operator()() const;
    operator bool() const;
    void reset();
  };

  struct DomainPreds {
    smt::OrExpr path;
    smt::DisjointExpr<smt::expr> UB;
    std::set<smt::expr> undef_vars;

    smt::expr operator()() const;
  };

  // TODO: make this const again
  Function &f;
  bool source;
  bool disable_undef_rewrite = false;
  bool is_initialization_phase = true;
  smt::AndExpr precondition, preconditionForApprox;
  smt::AndExpr axioms;
  smt::AndExpr ooms;

  const BasicBlock *current_bb;
  std::set<smt::expr> quantified_vars;

  // var -> ((value, not_poison), undef_vars, already_used?)
  std::unordered_map<const Value*, unsigned> values_map;
  std::vector<std::tuple<const Value*, ValTy, bool>> values;

  // dst BB -> src BB -> (domain data, memory)
  std::unordered_map<const BasicBlock*,
                     std::unordered_map<const BasicBlock*,
                                        std::pair<DomainPreds,
                                                  smt::DisjointExpr<Memory>>>>
    predecessor_data;
  std::unordered_set<const BasicBlock*> seen_bbs;

  // Global variables' memory block ids & Memory::alloc has been called?
  std::unordered_map<std::string, std::pair<unsigned, bool> > glbvar_bids;

  // temp state
  CurrentDomain domain;
  Memory memory;
  std::set<smt::expr> undef_vars;
  std::array<StateValue, 32> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  // return_domain: a boolean expression describing return condition
  smt::OrExpr return_domain;
  smt::DisjointExpr<StateValue> return_val;
  smt::DisjointExpr<Memory> return_memory;
  std::set<smt::expr> return_undef_vars;

  struct FnCallInput {
    std::vector<StateValue> args_nonptr;
    // (ptr arguments, is by_val arg?)
    std::vector<std::pair<StateValue, bool>> args_ptr;
    Memory m;
    bool readsmem, argmemonly;
    bool operator<(const FnCallInput &rhs) const {
      return std::tie(args_nonptr, args_ptr, m, readsmem, argmemonly) <
             std::tie(rhs.args_nonptr, rhs.args_ptr, rhs.m, rhs.readsmem,
                      rhs.argmemonly);
    }
  };
  struct FnCallOutput {
    std::vector<StateValue> retvals;
    smt::expr ub;
    Memory::CallState callstate;
    bool used;
  };
  std::map<std::string, std::map<FnCallInput, FnCallOutput>> fn_call_data;

public:
  State(Function &f, bool source);

  static void resetGlobals();

  const StateValue& exec(const Value &v);
  const StateValue& operator[](const Value &val);
  const StateValue& getAndAddUndefs(const Value &val);
  const ValTy& at(const Value &val) const;
  const smt::OrExpr* jumpCondFrom(const BasicBlock &bb) const;
  bool isUndef(const smt::expr &e) const;

  bool startBB(const BasicBlock &bb);
  void addJump(const BasicBlock &dst);
  // boolean cond
  void addJump(smt::expr &&cond, const BasicBlock &dst);
  // i1 cond
  void addCondJump(const smt::expr &cond, const BasicBlock &dst_true,
                   const BasicBlock &dst_false);
  void addReturn(const StateValue &val);

  void addAxiom(smt::expr &&axiom) { axioms.add(std::move(axiom)); }
  void addPre(smt::expr &&cond, bool forApprox = false)
  { (forApprox ? preconditionForApprox : precondition).add(std::move(cond)); }
  void addUB(smt::expr &&ub);
  void addUB(const smt::expr &ub);
  void addUB(smt::AndExpr &&ubs);
  void addOOM(smt::expr &&oom) { ooms.add(std::move(oom)); }

  const std::vector<StateValue>
    addFnCall(const std::string &name, std::vector<StateValue> &&inputs,
              std::vector<std::pair<StateValue, bool>> &&ptr_inputs,
              const std::vector<Type*> &out_types, bool reads_memory,
              bool writes_memory, bool argmemonly,
              std::vector<StateValue> &&returned_val);

  void addQuantVar(const smt::expr &var);
  void addUndefVar(smt::expr &&var);
  void resetUndefVars();

  StateValue rewriteUndef(StateValue &&val);

  bool isInitializationPhase() const { return is_initialization_phase; }
  void finishInitializer();

  auto& getFn() const { return f; }
  auto& getMemory() { return memory; }
  auto& getAxioms() const { return axioms; }
  auto& getPre(bool forApprox = false) const
  { return forApprox ? preconditionForApprox : precondition; }
  auto& getPreForApprox() const { return preconditionForApprox; }
  auto& getOOM() const { return ooms; }
  const auto& getValues() const { return values; }
  const auto& getQuantVars() const { return quantified_vars; }

  auto& returnDomain() const { return return_domain; }
  smt::expr sinkDomain() const;
  Memory returnMemory() const { return *return_memory(); }

  std::pair<StateValue, const std::set<smt::expr>&> returnVal() const {
    return { *return_val(), return_undef_vars };
  }

  void startParsingPre() { disable_undef_rewrite = true; }

  // whether this is source or target program
  bool isSource() const { return source; }

  void addGlobalVarBid(const std::string &glbvar, unsigned bid);
  // Checks whether glbvar has block id assigned.
  // If it has, bid is updated with the block id, and allocated is updated too.
  bool hasGlobalVarBid(const std::string &glbvar, unsigned &bid,
                       bool &allocated) const;
  void markGlobalAsAllocated(const std::string &glbvar);
  void syncSEdataWithSrc(const State &src);

  void mkAxioms(State &tgt);
  smt::expr simplifyWithAxioms(smt::expr &&e) const;

private:
  void addJump(const BasicBlock &dst, smt::expr &&domain);
};

}
