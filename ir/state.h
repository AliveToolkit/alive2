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
    smt::expr path = true; // path from fn entry
    smt::AndExpr UB;
    std::set<smt::expr> undef_vars;

    smt::expr operator()() const;
    operator bool() const;
  };

  struct ValueAnalysis {
    std::set<const Value *> non_poison_vals; // vars that are not poison
    // vars that are not undef (partially undefs are not allowed too)
    std::map<const Value *, smt::expr> non_undef_vals;
    // vars that have never been used
    std::set<const Value *> unused_vars;

    struct FnCallRanges
      : public std::map<std::string, std::pair<unsigned, unsigned>> {
      bool overlaps(const FnCallRanges &other) const;
    };
    FnCallRanges ranges_fn_calls;

    void intersect(const ValueAnalysis &other);
  };

  struct VarArgsEntry {
    smt::expr alive;
    smt::expr next_arg;
    smt::expr num_args;
    smt::expr is_va_start;
    smt::expr active; // false if this entry is repeated

    VarArgsEntry() {}
    VarArgsEntry(smt::expr &&alive, smt::expr &&next_arg, smt::expr &&num_args,
                 smt::expr &&is_va_start, smt::expr &&active)
      : alive(std::move(alive)), next_arg(std::move(next_arg)),
        num_args(std::move(num_args)), is_va_start(std::move(is_va_start)),
        active(std::move(active)) {}

    auto operator<=>(const VarArgsEntry &rhs) const = default;
  };

  struct VarArgsData {
    std::map<smt::expr, VarArgsEntry> data;
    static VarArgsData mkIf(const smt::expr &cond, const VarArgsData &then,
                            const VarArgsData &els);
    auto operator<=>(const VarArgsData &rhs) const = default;
  };

  struct BasicBlockInfo {
    smt::OrExpr path;
    smt::DisjointExpr<smt::expr> UB;
    smt::DisjointExpr<Memory> mem;
    std::set<smt::expr> undef_vars;
    ValueAnalysis analysis;
    VarArgsData var_args;
  };

  const Function &f;
  bool source;
  bool disable_undef_rewrite = false;
  bool is_initialization_phase = true;
  smt::AndExpr precondition;
  smt::AndExpr axioms;

  std::set<std::pair<std::string,std::optional<smt::expr>>> used_approximations;

  std::set<smt::expr> quantified_vars;

  // var -> ((value, not_poison), undef_vars)
  std::unordered_map<const Value*, unsigned> values_map;
  std::vector<std::pair<const Value*, ValTy>> values;

  // dst BB -> src BB -> BasicBlockInfo
  std::unordered_map<const BasicBlock*,
                     std::unordered_map<const BasicBlock*, BasicBlockInfo>>
    predecessor_data;
  std::unordered_set<const BasicBlock*> seen_bbs;

  // Global variables' memory block ids & Memory::alloc has been called?
  std::unordered_map<std::string, std::pair<unsigned, bool>> glbvar_bids;

  // temp state
  const BasicBlock *current_bb = nullptr;
  CurrentDomain domain;
  Memory memory;
  ValueAnalysis analysis;
  // temp state (reset after each State::exec)
  std::set<smt::expr> undef_vars;
  std::array<StateValue, 64> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  StateValue* no_more_tmp_slots();

  // return_domain: a boolean expression describing return condition
  smt::OrExpr return_domain;
  // function_domain: a condition for function having well-defined behavior
  smt::OrExpr function_domain;
  smt::DisjointExpr<StateValue> return_val;
  smt::DisjointExpr<Memory> return_memory;
  std::set<smt::expr> return_undef_vars;

  struct FnCallInput {
    std::vector<StateValue> args_nonptr;
    std::vector<Memory::PtrInput> args_ptr;
    ValueAnalysis::FnCallRanges fncall_ranges;
    Memory m;
    bool readsmem, argmemonly;

    smt::expr operator==(const FnCallInput &rhs) const;
    smt::expr refinedBy(State &s, const std::vector<StateValue> &args_nonptr,
                        const std::vector<Memory::PtrInput> &args_ptr,
                        const ValueAnalysis::FnCallRanges &fncall_ranges,
                        const Memory &m, bool readsmem, bool argmemonly) const;

    auto operator<=>(const FnCallInput &rhs) const = default;
  };

  struct FnCallOutput {
    std::vector<StateValue> retvals;
    smt::expr ub;
    Memory::CallState callstate;

    static FnCallOutput mkIf(const smt::expr &cond, const FnCallOutput &then,
                             const FnCallOutput &els);
    smt::expr operator==(const FnCallOutput &rhs) const;
    auto operator<=>(const FnCallOutput &rhs) const = default;
  };
  std::map<std::string, std::map<FnCallInput, FnCallOutput>> fn_call_data;
  smt::expr fn_call_pre = true;
  std::set<smt::expr> fn_call_qvars;

  VarArgsData var_args_data;

public:
  State(const Function &f, bool source);

  static void resetGlobals();

  /*--- Get values or update registers ---*/
  const StateValue& exec(const Value &v);
  const StateValue& operator[](const Value &val);
  const StateValue& getAndAddUndefs(const Value &val);
  // If undef_ub is true, UB is also added when val was undef
  const StateValue& getAndAddPoisonUB(const Value &val, bool undef_ub = false);

  const ValTy& at(const Value &val) const;

  // Functions for updating undef vars that are needed to represent Value being
  // executed
  void addUndefVar(smt::expr &&var);
  auto& getUndefVars() const { return undef_vars; }
  void resetUndefVars();

  StateValue rewriteUndef(StateValue &&val,
                          const std::set<smt::expr> &undef_vars);

  /*--- Control flow ---*/
  const smt::OrExpr* jumpCondFrom(const BasicBlock &bb) const;
  bool startBB(const BasicBlock &bb);
  void addJump(const BasicBlock &dst);
  // boolean cond
  void addJump(smt::expr &&cond, const BasicBlock &dst);
  // i1 cond
  void addCondJump(const smt::expr &cond, const BasicBlock &dst_true,
                   const BasicBlock &dst_false);
  void addReturn(StateValue &&val);

  /*--- Axioms, preconditions, domains ---*/
  void addAxiom(smt::AndExpr &&ands) { axioms.add(std::move(ands)); }
  void addAxiom(smt::expr &&axiom) { axioms.add(std::move(axiom)); }
  void addPre(smt::expr &&cond) { precondition.add(std::move(cond)); }
  void addUB(smt::expr &&ub);
  void addUB(const smt::expr &ub);
  void addUB(smt::AndExpr &&ubs);
  void addNoReturn();

  std::vector<StateValue>
    addFnCall(const std::string &name, std::vector<StateValue> &&inputs,
              std::vector<Memory::PtrInput> &&ptr_inputs,
              const std::vector<Type*> &out_types, const FnAttrs &attrs);

  auto& getVarArgsData() { return var_args_data.data; }

  void doesApproximation(std::string &&name, std::optional<smt::expr> e = {});
  auto& getApproximations() const { return used_approximations; }

  void addQuantVar(const smt::expr &var);
  void addFnQuantVar(const smt::expr &var);

  bool isInitializationPhase() const { return is_initialization_phase; }
  void finishInitializer();

  auto& getFn() const { return f; }
  auto& getMemory() { return memory; }
  auto& getAxioms() const { return axioms; }
  auto& getPre() const { return precondition; }
  auto& getFnPre() const { return fn_call_pre; }
  const auto& getValues() const { return values; }
  const auto& getQuantVars() const { return quantified_vars; }
  const auto& getFnQuantVars() const { return fn_call_qvars; }

  auto& functionDomain() const { return function_domain; }
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

private:
  smt::expr stripUndefAndAddUB(const Type &valty, const smt::expr &e);
  bool isUndefVarInCurrentExec(const smt::expr &e) const;
  void addJump(const BasicBlock &dst, smt::expr &&domain);
};

}
