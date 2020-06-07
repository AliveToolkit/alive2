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
class CFG;
class DomTree;

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

  const BasicBlock *current_bb = nullptr;
  std::set<smt::expr> quantified_vars;

  // var -> ((value, not_poison), undef_vars, already_used?)
  std::unordered_map<const Value*, unsigned> values_map;
  std::vector<std::tuple<const Value*, ValTy, bool>> values;

  // dst BB -> src BB -> (domain data, memory, jump_cond)
  std::unordered_map<const BasicBlock*,
                     std::unordered_map<const BasicBlock*,
                                        std::tuple<DomainPreds,
                                                   smt::DisjointExpr<Memory>,
                                                   std::optional<smt::expr>>>>
    predecessor_data;
  std::unordered_set<const BasicBlock*> seen_bbs;

  // Global variables' memory block ids & Memory::alloc has been called?
  std::unordered_map<std::string, std::pair<unsigned, bool> > glbvar_bids;

  // temp state
  CurrentDomain domain;
  smt::AndExpr isolated_ub;
  Memory memory;
  std::set<smt::expr> undef_vars;
  std::array<StateValue, 32> tmp_values;
  unsigned i_tmp_values = 0; // next available position in tmp_values

  // return_domain: a boolean expression describing return condition
  smt::OrExpr return_domain;
  // boolean expression describing all possible return paths only
  smt::OrExpr return_path;
  // function_domain: a condition for function having well-defined behavior
  smt::OrExpr function_domain; 
  smt::DisjointExpr<StateValue> return_val;
  smt::DisjointExpr<Memory> return_memory;
  std::set<smt::expr> return_undef_vars;

  struct FnCallInput {
    std::vector<StateValue> args_nonptr;
    std::vector<Memory::PtrInput> args_ptr;
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

  // src -> <(dst, cond), isolated ub>
  struct TargetData {
    std::vector<std::pair<const BasicBlock*, smt::expr>> dsts;
    std::optional<smt::expr> ub;
  };
  std::unordered_map<const BasicBlock*, TargetData> global_target_data;
  
  // data structure to hold temporary UB when constructing it in buildUB()
  struct BuildUBData {
    bool visited;
    std::optional<smt::expr> ub;
    std::optional<smt::expr> carry_ub;
  };
  // dominator tree
  std::unique_ptr<DomTree> dom_tree;
  std::unique_ptr<CFG> cfg;

  // if false after SE, buildUB sets function_domain to return_domain
  bool has_noreturn = false;
  // a set to hold bb's during SE that do not lead to a return	
  // either they reach unreachable or a jump instruction with only back-edges	
  std::unordered_set<const BasicBlock*> no_ret_bbs;	
  std::unordered_map<const BasicBlock*,unsigned> back_edge_counter;
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
  bool canMoveExprsToDom(const BasicBlock &merge, const BasicBlock &dom);
  
  void buildTargetData(std::unordered_map<const BasicBlock*, State::TargetData> 
                       *tdata, const BasicBlock &end);
  smt::expr buildUB();
  smt::expr buildUB(std::unordered_map<const BasicBlock*, TargetData> *tdata);
  auto* targetData() { return &global_target_data; }
  auto& returnPath() { return return_path; }
  void setReturnDomain(smt::expr &&ret_dom);
  void setFunctionDomain(const smt::expr &f_dom);
  bool foundReturn() const { return !return_val.empty(); }
  bool foundNoReturnAttr() const { return has_noreturn; }

  void propagateNoRetBB(const BasicBlock &bb);	
  const BasicBlock& getCurrentBB() const { return *current_bb; }

  void addJump(const BasicBlock &dst);
  // boolean cond
  void addJump(smt::expr &&cond, const BasicBlock &dst);
  // i1 cond
  void addCondJump(const smt::expr &cond, const BasicBlock &dst_true,
                   const BasicBlock &dst_false);
  void addReturn(const StateValue &val);

  void addAxiom(smt::AndExpr &&ands) { axioms.add(std::move(ands)); }
  void addAxiom(smt::expr &&axiom) { axioms.add(std::move(axiom)); }
  void addPre(smt::expr &&cond, bool forApprox = false)
  { (forApprox ? preconditionForApprox : precondition).add(std::move(cond)); }
  void addUB(smt::expr &&ub);
  void addUB(const smt::expr &ub);
  void addUB(smt::AndExpr &&ubs);
  void addNoReturn();
  void addOOM(smt::expr &&oom) { ooms.add(std::move(oom)); }

  const std::vector<StateValue>
    addFnCall(const std::string &name, std::vector<StateValue> &&inputs,
              std::vector<Memory::PtrInput> &&ptr_inputs,
              const std::vector<Type*> &out_types, const FnAttrs &attrs);

  void addQuantVar(const smt::expr &var);
  void addUndefVar(smt::expr &&var);
  auto& getUndefVars() const { return undef_vars; }
  void resetUndefVars();

  StateValue rewriteUndef(StateValue &&val,
                          const std::set<smt::expr> &undef_vars);

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
  smt::expr simplifyWithAxioms(smt::expr &&e) const;

private:
  void addJump(const BasicBlock &dst, smt::expr &&domain);
};

}
