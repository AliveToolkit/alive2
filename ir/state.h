#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/attrs.h"
#include "ir/memory.h"
#include "ir/state_value.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include <array>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace IR {

class Value;
class JumpInstr;
class BasicBlock;
class Function;

class State {
public:
  struct ValTy {
    StateValue val;
    smt::expr return_domain;
    smt::expr domain;
    std::set<smt::expr> undef_vars;
  };

private:
  struct CurrentDomain {
    smt::expr path = true; // path from fn entry
    smt::AndExpr UB;
    smt::expr noreturn;
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

    // Possible number of calls per function name that occurred so far
    // This is an over-approximation, union over all predecessors
    struct FnCallRanges
      : public std::map<std::string, std::pair<std::set<unsigned>,
                        MemoryAccess>> {
      void inc(const std::string &name, MemoryAccess access);
      bool overlaps(const std::string &callee, MemoryAccess call_access,
                    const FnCallRanges &other) const;
      // remove all ranges but name
      FnCallRanges project(const std::string &name) const;
    };
    FnCallRanges ranges_fn_calls;

    void meet_with(const ValueAnalysis &other);
  };

  struct VarArgsEntry {
    smt::expr alive;
    smt::expr next_arg;
    smt::expr num_args;
    smt::expr is_va_start;
    smt::expr active; // false if this entry is repeated

    VarArgsEntry() = default;
    VarArgsEntry(smt::expr &&alive, smt::expr &&next_arg, smt::expr &&num_args,
                 smt::expr &&is_va_start, smt::expr &&active)
      : alive(std::move(alive)), next_arg(std::move(next_arg)),
        num_args(std::move(num_args)), is_va_start(std::move(is_va_start)),
        active(std::move(active)) {}

    auto operator<=>(const VarArgsEntry &rhs) const = default;
  };

  struct VarArgsData {
    std::map<smt::expr, VarArgsEntry> data;
    static VarArgsData mkIf(const smt::expr &cond, VarArgsData &&then,
                            VarArgsData &&els);
    auto operator<=>(const VarArgsData &rhs) const = default;
  };

  struct BasicBlockInfo {
    smt::OrExpr path;
    smt::DisjointExpr<smt::expr> UB, guardUB;
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

  // for -disallow-ub-exploitation
  smt::OrExpr unreachable_paths;

  std::set<std::pair<std::string,std::optional<smt::expr>>> used_approximations;

  std::set<smt::expr> quantified_vars;
  std::set<smt::expr> nondet_vars;

  // var -> ((value, not_poison), ub, undef_vars)
  std::unordered_map<const Value*, unsigned> values_map;
  std::vector<std::pair<const Value*, ValTy>> values;

  // dst BB -> src BB -> BasicBlockInfo
  std::unordered_map<const BasicBlock*,
                     std::unordered_map<const BasicBlock*, BasicBlockInfo>>
    predecessor_data;
  std::unordered_set<const BasicBlock*> seen_bbs;

  // Global variables' memory block ids & Memory::alloc has been called?
  std::unordered_map<std::string, std::pair<unsigned, bool>> glbvar_bids;

  // The value of a 'returned' input
  std::optional<StateValue> returned_input;

  // temp state
  const BasicBlock *current_bb = nullptr;
  CurrentDomain domain;
  Memory memory;
  smt::expr fp_rounding_mode;
  std::set<smt::expr> undef_vars;
  ValueAnalysis analysis;
  std::array<StateValue, 64> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  void check_enough_tmp_slots();

  // return_domain: a boolean expression describing return condition
  smt::OrExpr return_domain;
  // function_domain: a condition for function having well-defined behavior
  smt::OrExpr function_domain;
  smt::AndExpr guardable_ub;
  std::variant<smt::DisjointExpr<StateValue>, StateValue> return_val;
  std::variant<smt::DisjointExpr<Memory>, Memory> return_memory;
  std::set<smt::expr> return_undef_vars;

  struct FnCallInput {
    std::vector<StateValue> args_nonptr;
    std::vector<Memory::PtrInput> args_ptr;
    ValueAnalysis::FnCallRanges fncall_ranges;
    Memory m;
    MemoryAccess memaccess;
    bool noret, willret;

    smt::expr operator==(const FnCallInput &rhs) const;
    smt::expr refinedBy(State &s, const std::string &callee,
                        unsigned inaccessible_bid,
                        const std::vector<StateValue> &args_nonptr,
                        const std::vector<Memory::PtrInput> &args_ptr,
                        const ValueAnalysis::FnCallRanges &fncall_ranges,
                        const Memory &m, MemoryAccess memaccess, bool noret,
                        bool willret) const;

    auto operator<=>(const FnCallInput &rhs) const = default;
  };

  struct FnCallOutput {
    std::vector<StateValue> retvals;
    smt::expr ub;
    smt::expr noreturns;
    Memory::CallState callstate;
    std::vector<Memory::FnRetData> ret_data;

    static FnCallOutput mkIf(const smt::expr &cond, const FnCallOutput &then,
                             const FnCallOutput &els);
    smt::expr operator==(const FnCallOutput &rhs) const;
    auto operator<=>(const FnCallOutput &rhs) const = default;
  };
  std::map<std::string, std::map<FnCallInput, FnCallOutput>> fn_call_data;
  smt::expr fn_call_pre = true;
  std::set<smt::expr> fn_call_qvars;
  std::unordered_map<std::string, unsigned> inaccessiblemem_bids;

  VarArgsData var_args_data;

  const StateValue& returnValCached();

public:
  State(const Function &f, bool source);

  static void resetGlobals();

  /*--- Get values or update registers ---*/
  const ValTy& exec(const Value &v);
  const StateValue& eval(const Value &val, bool quantify_nondet);
  const StateValue& operator[](const Value &val) { return eval(val, false); }
  const StateValue& getAndAddUndefs(const Value &val);
  // If undef_ub is true, UB is also added when val was undef
  const StateValue& getAndAddPoisonUB(const Value &val, bool undef_ub = false,
                                      bool ptr_compare = false);
  const StateValue& getVal(const Value &val, bool is_poison_ub);
  const smt::expr& getWellDefinedPtr(const Value &val);

  const ValTy& at(const Value &val) const;
  bool isUndef(const smt::expr &e) const;

  /*--- Control flow ---*/
  const smt::OrExpr* jumpCondFrom(const BasicBlock &bb) const;
  bool startBB(const BasicBlock &bb);
  void addJump(const BasicBlock &dst);
  // boolean cond
  void addJump(smt::expr &&cond, const BasicBlock &dst, bool last_jump = false);
  // i1 cond
  void addCondJump(const smt::expr &cond, const BasicBlock &dst_true,
                   const BasicBlock &dst_false);
  void addReturn(StateValue &&val);

  /*--- Axioms, preconditions, domains ---*/
  void addAxiom(smt::AndExpr &&ands) { axioms.add(std::move(ands)); }
  void addAxiom(smt::expr &&axiom) { axioms.add(std::move(axiom)); }
  void addPre(smt::expr &&cond) { precondition.add(std::move(cond)); }

  // we have 2 types of UB to support -disallow-ub-exploitation
  // 1) UB that cannot be safeguarded, and 2) UB that can be safeguarded
  // The 2nd type is not allowed.
  void addUB(std::pair<smt::AndExpr, smt::expr> &&ub);
  void addUB(smt::expr &&ub);
  void addUB(smt::AndExpr &&ubs);
  void addGuardableUB(smt::expr &&ub);

  void addUnreachable();
  void addNoReturn(const smt::expr &cond);
  bool isViablePath() const { return domain.UB; }

  std::vector<StateValue>
    addFnCall(const std::string &name, std::vector<StateValue> &&inputs,
              std::vector<Memory::PtrInput> &&ptr_inputs,
              const std::vector<Type*> &out_types, const FnAttrs &attrs);

  auto& getVarArgsData() { return var_args_data.data; }

  void doesApproximation(std::string &&name, std::optional<smt::expr> e = {});
  auto& getApproximations() const { return used_approximations; }

  smt::expr getFreshNondetVar(const char *prefix, const smt::expr &type);
  void addQuantVar(const smt::expr &var);
  void addFnQuantVar(const smt::expr &var);
  void addUndefVar(smt::expr &&var);
  auto& getUndefVars() const { return undef_vars; }
  void resetUndefVars();

  StateValue rewriteUndef(StateValue &&val,
                          const std::set<smt::expr> &undef_vars);
  smt::expr rewriteUndef(smt::expr &&val,
                         const std::set<smt::expr> &undef_vars);

  bool isInitializationPhase() const { return is_initialization_phase; }
  void finishInitializer();

  auto& getFn() const { return f; }
  auto& getMemory() const { return memory; }
  auto& getMemory() { return memory; }
  auto& getFpRoundingMode() const { return fp_rounding_mode; }
  auto& getAxioms() const { return axioms; }
  auto& getPre() const { return precondition; }
  auto& getFnPre() const { return fn_call_pre; }
  auto& getUnreachable() const { return unreachable_paths; }
  const auto& getValues() const { return values; }
  const auto& getQuantVars() const { return quantified_vars; }
  const auto& getNondetVars() const { return nondet_vars; }
  const auto& getFnQuantVars() const { return fn_call_qvars; }

  void saveReturnedInput();
  const std::optional<StateValue>& getReturnedInput() const {
    return returned_input;
  }

  smt::expr sinkDomain() const;
  Memory& returnMemory();

  ValTy returnVal() {
    return { returnValCached(), return_domain(), function_domain(),
             return_undef_vars };
  }

  smt::expr getGuardableUB() const { return guardable_ub(); }

  smt::expr getJumpCond(const BasicBlock &src, const BasicBlock &dst) const;

  void startParsingPre() { disable_undef_rewrite = true; }

  // whether this is source or target program
  bool isSource() const { return source; }

  void addGlobalVarBid(const std::string &glbvar, unsigned bid);
  // Checks whether glbvar has block id assigned.
  // If it has, bid is updated with the block id, and allocated is updated too.
  bool hasGlobalVarBid(const std::string &glbvar, unsigned &bid,
                       bool &allocated) const;
  void markGlobalAsAllocated(const std::string &glbvar);
  void syncSEdataWithSrc(State &src);

  void mkAxioms(State &tgt);

private:
  smt::expr strip_undef_and_add_ub(const Value &val, const smt::expr &e,
                                   bool isptr);
};

}
