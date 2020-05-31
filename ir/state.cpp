// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/function.h"
#include "ir/globals.h"
#include "smt/smt.h"
#include "util/errors.h"
#include <cassert>
#include <stack>

using namespace smt;
using namespace util;
using namespace std;

namespace IR {

expr State::CurrentDomain::operator()() const {
  return path && UB();
}

State::CurrentDomain::operator bool() const {
  return !path.isFalse() && UB;
}

void State::CurrentDomain::reset() {
  path = true;
  UB.reset();
  undef_vars.clear();
}

expr State::DomainPreds::operator()() const {
  return path() && *UB();
}


State::State(Function &f, bool source)
  : f(f), source(source), memory(*this),
    return_val(f.getType().getDummyValue(false)), return_memory(memory) {}

void State::resetGlobals() {
  Memory::resetBids(has_null_block);
}

const StateValue& State::exec(const Value &v) {
  assert(undef_vars.empty());
  auto val = v.toSMT(*this);
  ENSURE(values_map.try_emplace(&v, (unsigned)values.size()).second);
  values.emplace_back(&v, ValTy(move(val), move(undef_vars)), false);

  // cleanup potentially used temporary values due to undef rewriting
  while (i_tmp_values > 0) {
    tmp_values[--i_tmp_values] = StateValue();
  }

  return get<1>(values.back()).first;
}

const StateValue& State::operator[](const Value &val) {
  auto &[var, val_uvars, used] = values[values_map.at(&val)];
  auto &[sval, uvars] = val_uvars;
  (void)var;

  if (uvars.empty() || !used || disable_undef_rewrite) {
    used = true;
    undef_vars.insert(uvars.begin(), uvars.end());
    return sval;
  }

  vector<pair<expr, expr>> repls;
  for (auto &u : uvars) {
    repls.emplace_back(u, expr::mkFreshVar("undef", u));
  }

  if (hit_half_memory_limit())
    throw AliveException("Out of memory; skipping function.", false);

  auto sval_new = sval.subst(repls);
  if (sval_new.eq(sval)) {
    uvars.clear();
    return sval;
  }

  for (auto &p : repls) {
    undef_vars.emplace(move(p.second));
  }

  return tmp_values[i_tmp_values++] = move(sval_new);
}

const StateValue& State::getAndAddUndefs(const Value &val) {
  auto &v = (*this)[val];
  for (auto uvar: at(val).second)
    addQuantVar(move(uvar));
  return v;
}

const State::ValTy& State::at(const Value &val) const {
  return get<1>(values[values_map.at(&val)]);
}

const OrExpr* State::jumpCondFrom(const BasicBlock &bb) const {
  auto &pres = predecessor_data.at(current_bb);
  auto I = pres.find(&bb);
  return I == pres.end() ? nullptr : &std::get<0>(I->second).path;
}

bool State::isUndef(const expr &e) const {
  return undef_vars.count(e) != 0;
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  ENSURE(seen_bbs.emplace(&bb).second);
  current_bb = &bb;

  domain.reset();
  isolated_ub.reset();

  if (&f.getFirstBB() == &bb)
    return true;

  auto I = predecessor_data.find(&bb);
  if (I == predecessor_data.end())
    return false;

  DisjointExpr<Memory> in_memory;
  DisjointExpr<expr> UB;
  OrExpr path;

  for (auto &[src, data] : I->second) {
    (void)src;
    auto &[dom, mem, cond] = data;
    (void)cond;
    path.add(dom.path);
    expr p = dom.path();
    UB.add_disj(dom.UB, p);
    in_memory.add_disj(mem, dom.path());
    domain.undef_vars.insert(dom.undef_vars.begin(), dom.undef_vars.end());
  }

  domain.path = path();
  domain.UB.add(*UB());
  memory = *in_memory();

  return domain;
}

bool State::canMoveExprsToDom(const BasicBlock &merge, const BasicBlock &dom) {
  unordered_map<const BasicBlock*, unordered_set<const BasicBlock*>> 
    bb_seen_targets;
  unordered_map<const BasicBlock*, bool> v;
  stack<const BasicBlock*> S;
  S.push(&merge);
  auto cur_bb = &merge;

  while(!S.empty()) {
    cur_bb = S.top();
    S.pop();
    auto &visited = v[cur_bb];
    
    if (!visited && cur_bb != &dom) {
      visited = true;

      for (auto const &pred : predecessor_data[cur_bb]) {
        bb_seen_targets[pred.first].insert(cur_bb);
        S.push(pred.first);
      }
    }
  }

  for (auto &[bb, seen_targets] : bb_seen_targets) {
    auto jmp_instr = static_cast<JumpInstr*>(bb->back());
    auto tgt_count = jmp_instr->getTargetCount();
   
    // only count unique targets for switches
    if (auto sw = dynamic_cast<Switch*>(bb->back()))
      tgt_count = sw->getNumUniqueTargets();
    
    if (seen_targets.size() != tgt_count) {
      // If condition fails double check to exclude bb's with unreachable
      // from counting
      auto tgts = jmp_instr->targets();
      for (auto I = tgts.begin(), E = tgts.end(); I != E; ++I) {
        // if target has no UB, then it was ignored in buildUB and thus here too
        if (no_ret_bbs.find(&(*I)) != no_ret_bbs.end())
          --tgt_count;
      }
      if (seen_targets.size() != tgt_count)
        return false;
    }
  }
  return true;
}

// Fill a map with target data for a subset of nodes of the CFG starting from
// some specified BasicBlock
void State::buildTargetData(unordered_map<const BasicBlock*, State::TargetData>
                            *tdata, const BasicBlock &end) {
  unordered_map<const BasicBlock*, bool> v;
  stack<const BasicBlock*> S;
  S.push(&end);

  while (!S.empty()) {
    auto cur_bb = S.top();
    S.pop();

    auto &visited = v[cur_bb];
    if (visited)
      continue;

    for (auto &[pred, pred_data] : predecessor_data[cur_bb]) {
      S.push(pred);
      auto &p_tdata = (*tdata)[pred];
      p_tdata.dsts.emplace_back(cur_bb, *get<2>(pred_data));
      p_tdata.ub = global_target_data[pred].ub;
    }
  }
}

expr State::buildUB() {
  return buildUB(&global_target_data);
}

// Traverse the program graph similar to DFS to build UB as an ite expr tree
expr State::buildUB(unordered_map<const BasicBlock*, TargetData> *tdata) {
  std::unordered_map<const BasicBlock*, BuildUBData> build_ub_data;
  stack<const BasicBlock*> S;
  S.push(&f.getFirstBB());

  // 2 phases:
  // 1º - visit and push tgts of this bb to ensure traversal of sucessors first
  // 2º - visit and build the ub as ite's 
  while (!S.empty()) {
    auto cur_bb = S.top();
    S.pop();

    auto &[visited, ub, carry_ub] = build_ub_data[cur_bb];
    if (!visited) {
      visited = true;
      S.push(cur_bb);

      // targets in target_data do not include back-edges or jmps to #sink
      auto &cur_data = (*tdata)[cur_bb];
      for (auto &[tgt_bb, cond] : cur_data.dsts) {
        (void)cond;
        S.push(tgt_bb);
      }
    } else {
      if (!ub) {
        auto &cur_data = (*tdata)[cur_bb];
        
        // skip bb's that do not have set ub (ex: bb only has back-edges)
        if (!cur_data.ub)
          continue;
        
        // build ub for targets
        for (auto &[tgt_bb, cond] : cur_data.dsts) {
          auto &tgt_ub = build_ub_data[tgt_bb].ub;
          if (tgt_ub)
            ub = ub ? expr::mkIf(cond, *tgt_ub, *ub) : tgt_ub;
        }
        
        // only add current ub if at least one target has set ub
        if (ub || cur_data.dsts.empty()) {
          // 'and' the isolated ub of cur_bb with the (if built) targets ub
          auto &cur_ub = cur_data.ub;
          ub = ub ? *ub && *cur_ub : cur_ub;
          if (carry_ub)
            ub = *ub && *carry_ub;

          auto pred_data = predecessor_data[cur_bb];
          if (pred_data.size() > 1) {
            if (!dom_tree) {
              cfg = make_unique<CFG>(f);
              dom_tree = make_unique<DomTree>(f, *cfg);
            }

            auto &dom = *dom_tree->getIDominator(*cur_bb);
            if (canMoveExprsToDom(*cur_bb, dom)) {
              build_ub_data[&dom].carry_ub = move(ub);
              ub = true;
            }
          }
        }
      }
    }
  }
  return *build_ub_data[&f.getFirstBB()].ub;
}

// walk up the CFG to add bb's to no_ret_bbs which when reached will always	
// lead execution to an unreachable or a back-edge	
// this is useful for ignoring unecessary paths quickly by checking no_ret_bbs	
void State::propagateNoRetBB(const BasicBlock &bb) {	
  stack<const BasicBlock*> S;	

  no_ret_bbs.insert(&bb);	
  for (auto &[pred, data] : predecessor_data[&bb])	
    S.push(pred);	

  while (!S.empty()) {	
    auto cur_bb = S.top();	
    S.pop();	

    // TODO when coming from assume 0, last instr is not a jump, but an assume	
    // which means this code is wrong and may not add the bb to no_ret_bbs properly	
    // maybe add a flag to the parameters or dynamic cast to assume first before trying jump	
    auto jmp_instr = static_cast<JumpInstr*>(cur_bb->back());	
    if (jmp_instr->getTargetCount() == 1) {	
      no_ret_bbs.insert(cur_bb);	
      for (auto &[pred, data] : predecessor_data[cur_bb])	
        S.push(pred);	
    } else {	
      unsigned no_ret_cnt = 0;	
      auto jmp_tgts = jmp_instr->targets();	
      for (auto I = jmp_tgts.begin(), E = jmp_tgts.end(); I != E; ++I) {	
        if (no_ret_bbs.find(&(*I)) != no_ret_bbs.end())	
          ++no_ret_cnt;	
      }	
      if (no_ret_cnt == jmp_instr->getTargetCount()) {	
        no_ret_bbs.insert(cur_bb);	
        for (auto &[pred, data] : predecessor_data[cur_bb])	
          S.push(pred);	
      }	
    }	
  }	
}

void State::setReturnDomain(expr &&ret_dom) {
  return_domain.reset();
  return_domain.add(move(ret_dom));
}

void State::setFunctionDomain(const expr &f_dom) {
  function_domain.reset();
  function_domain.add(f_dom);
}

void State::addJump(const BasicBlock &dst0, expr &&cond) {
  if (cond.isFalse())
    return;

  auto dst = &dst0;
  if (seen_bbs.count(dst)) {
    dst = &f.getBB("#sink");
    auto &cnt = back_edge_counter[current_bb];	
    ++cnt;	
    auto jump_instr = static_cast<JumpInstr*>(current_bb->back());	
    auto tgt_count = jump_instr->getTargetCount();	
    // in case of switch use unique target count	
    if (auto sw = dynamic_cast<Switch*>(jump_instr))	
      tgt_count = sw->getNumUniqueTargets();	

    if (cnt == tgt_count) {	
      propagateNoRetBB(*current_bb);	
      back_edge_counter.erase(current_bb);	
    }
  }

  auto &data = predecessor_data[dst][current_bb];
  auto &domain_preds = get<0>(data);
  auto &mem = get<1>(data);
  auto &c = get<2>(data);
  c = c ? *c || cond : cond; // when switch default and case have same target

  // ignore #sink or back edges when building UB
  if (dst == &dst0) {
    auto &tgt_data = global_target_data[current_bb];
    tgt_data.dsts.emplace_back(dst, *c);
    tgt_data.ub = isolated_ub();
  }

  cond &= domain.path;
  mem.add(memory, cond);
  domain_preds.UB.add(domain.UB(), cond);
  domain_preds.path.add(move(cond));
  domain_preds.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  domain_preds.undef_vars.insert(domain.undef_vars.begin(),
                                 domain.undef_vars.end());
}

void State::addJump(const BasicBlock &dst) {
  addJump(dst, true);
  addUB(expr(false));
}

void State::addJump(expr &&cond, const BasicBlock &dst) {
  addJump(dst, move(cond));
}

void State::addCondJump(const expr &cond, const BasicBlock &dst_true,
                        const BasicBlock &dst_false) {
  addJump(dst_true,  cond == 1);
  addJump(dst_false, cond == 0);
  addUB(expr(false));
}

void State::addReturn(const StateValue &val) {
  global_target_data[current_bb].ub = isolated_ub();
  return_val.add(val, domain.path);
  return_memory.add(memory, domain.path);
  return_domain.add(domain());
  return_path.add(domain.path);
  function_domain.add(domain());
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  undef_vars.clear();
  addUB(expr(false));
}

void State::addUB(expr &&ub) {
  isolated_ub.add(ub);
  domain.UB.add(move(ub));
  if (!ub.isConst())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(const expr &ub) {
  isolated_ub.add(ub);
  domain.UB.add(ub);
  if (!ub.isConst())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(AndExpr &&ubs) {
  isolated_ub.add(ubs);
  domain.UB.add(ubs);
  domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addNoReturn() {
  function_domain.add(domain());
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  undef_vars.clear();
  addUB(expr(false));
  has_noreturn = true;
}

const vector<StateValue>
State::addFnCall(const string &name, vector<StateValue> &&inputs,
                 vector<Memory::PtrInput> &&ptr_inputs,
                 const vector<Type*> &out_types, const FnAttrs &attrs) {
  // TODO: handle changes to memory due to fn call
  // TODO: can read/write=false fn calls be removed?

  bool reads_memory = !attrs.has(FnAttrs::NoRead);
  bool writes_memory = !attrs.has(FnAttrs::NoWrite);
  bool argmemonly = attrs.has(FnAttrs::ArgMemOnly);

  bool all_valid = true;
  for (auto &v : inputs) {
    all_valid &= v.isValid();
  }
  for (auto &v : ptr_inputs) {
    all_valid &= v.val.isValid();
  }

  if (!all_valid) {
    addUB(expr());
    return vector<StateValue>(out_types.size());
  }

  // TODO: this doesn't need to compare the full memory, just a subset of fields
  auto call_data_pair
    = fn_call_data[name].try_emplace({ move(inputs), move(ptr_inputs),
                                       memory, reads_memory, argmemonly });
  auto &I = call_data_pair.first;
  bool inserted = call_data_pair.second;

  auto mk_val = [&](const Type &t, const string &name) {
    if (t.isPtrType())
      return memory.mkFnRet(name.c_str(), I->first.args_ptr);

    auto v = expr::mkFreshVar(name.c_str(), t.getDummyValue(false).value);
    return make_pair(v, v);
  };

  if (inserted) {
    vector<StateValue> values;
    string valname = name + "#val";
    string npname = name + "#np";
    for (auto t : out_types) {
      values.emplace_back(mk_val(*t, valname).first,
                          expr::mkFreshVar(npname.c_str(), false));
    }

    string ub_name = string(name) + "#ub";
    I->second = { move(values), expr::mkFreshVar(ub_name.c_str(), false),
                  writes_memory
                  ? memory.mkCallState(argmemonly ? &I->first.args_ptr : nullptr)
                  : Memory::CallState(), true };
  } else {
    I->second.used = true;
  }

  addUB(I->second.ub);

  if (writes_memory)
    memory.setState(I->second.callstate);

  return I->second.retvals;
}

void State::addQuantVar(const expr &var) {
  quantified_vars.emplace(var);
}

void State::addUndefVar(expr &&var) {
  undef_vars.emplace(move(var));
}

void State::resetUndefVars() {
  quantified_vars.insert(undef_vars.begin(), undef_vars.end());
  undef_vars.clear();
}

StateValue State::rewriteUndef(StateValue &&val, const set<expr> &undef_vars) {
  if (undef_vars.empty())
    return move(val);
  if (hit_half_memory_limit())
    throw AliveException("Out of memory; skipping function.", false);

  vector<pair<expr, expr>> repls;
  for (auto &var : undef_vars) {
    auto newvar = expr::mkFreshVar("undef", var);
    repls.emplace_back(var, newvar);
    addUndefVar(move(newvar));
  }
  return val.subst(repls);
}

void State::finishInitializer() {
  is_initialization_phase = false;
  memory.finishInitialization();
}

expr State::sinkDomain() const {
  auto bb = f.getBBIfExists("#sink");
  if (!bb)
    return false;

  auto I = predecessor_data.find(bb);
  if (I == predecessor_data.end())
    return false;

  OrExpr ret;
  for (auto &[src, data] : I->second) {
    (void)src;
    ret.add(std::get<0>(data).path());
  }
  return ret();
}

void State::addGlobalVarBid(const string &glbvar, unsigned bid) {
  ENSURE(glbvar_bids.emplace(glbvar, make_pair(bid, true)).second);
}

bool State::hasGlobalVarBid(const string &glbvar, unsigned &bid,
                            bool &allocated) const {
  auto itr = glbvar_bids.find(glbvar);
  bool found = itr != glbvar_bids.end();
  if (found) {
    bid = itr->second.first;
    allocated = itr->second.second;
  }
  return found;
}

void State::markGlobalAsAllocated(const string &glbvar) {
  auto itr = glbvar_bids.find(glbvar);
  assert(itr != glbvar_bids.end());
  itr->second.second = true;
}

void State::syncSEdataWithSrc(const State &src) {
  assert(glbvar_bids.empty());
  assert(src.isSource() && !isSource());
  glbvar_bids = src.glbvar_bids;
  for (auto &itm : glbvar_bids)
    itm.second.second = false;

  fn_call_data = src.fn_call_data;
  for (auto &[fn, map] : fn_call_data) {
   (void)fn;
    for (auto &[in, data] : map) {
      (void)in;
      data.used = false;
    }
  }

  // The bid of tgt global starts with num_nonlocals_src
  Memory::resetBids(num_nonlocals_src);
}

void State::mkAxioms(State &tgt) {
  assert(isSource() && !tgt.isSource());
  returnMemory().mkAxioms(tgt.returnMemory());

  // axioms for function calls
  // We would potentially need to do refinement check of tgt x tgt, but it
  // doesn't seem to be needed in practice for optimizations
  // since there's no introduction of calls in tgt
  for (auto &[fn, data] : fn_call_data) {
    auto &data2 = tgt.fn_call_data.at(fn);

    for (auto I = data.begin(), E = data.end(); I != E; ++I) {
      auto &[ins, ptr_ins, mem, reads, argmem] = I->first;
      auto &[rets, ub, mem_state, used] = I->second;
      assert(used); (void)used;

      for (auto I2 = data2.begin(), E2 = data2.end(); I2 != E2; ++I2) {
        auto &[ins2, ptr_ins2, mem2, reads2, argmem2] = I2->first;
        auto &[rets2, ub2, mem_state2, used2] = I2->second;

        if (!used2 || reads != reads2 || argmem != argmem2)
          continue;

        expr refines(true);
        for (unsigned i = 0, e = ins.size(); i != e; ++i) {
          refines &= ins[i].non_poison.implies(ins[i].value == ins2[i].value &&
                                               ins2[i].non_poison);
        }

        if (refines.isFalse())
          continue;

        for (unsigned i = 0, e = ptr_ins.size(); i != e; ++i) {
          // TODO: needs to take read/read2 as input to control if mem blocks
          // need to be compared
          auto &[ptr_in, is_byval] = ptr_ins[i];
          auto &[ptr_in2, is_byval2] = ptr_ins2[i];
          if (!is_byval && is_byval2) {
            // byval is added at target; this is not supported yet.
            refines = false;
            break;
          }
          expr eq_val = Pointer(mem, ptr_in.value)
                      .fninputRefined(Pointer(mem2, ptr_in2.value), is_byval2);
          refines &= ptr_in.non_poison
                       .implies(eq_val && ptr_in2.non_poison);

          if (refines.isFalse())
            break;
        }

        if (refines.isFalse())
          continue;

        if (reads2) {
          auto restrict_ptrs = argmem2 ? &ptr_ins2 : nullptr;
          expr mem_refined = mem.refined(mem2, true, restrict_ptrs).first;
          refines &= mem_refined;
        }

        expr ref_expr(true);
        for (unsigned i = 0, e = rets.size(); i != e; ++i) {
          ref_expr &=
            rets[i].non_poison.implies(rets2[i].non_poison &&
                                       rets[i].value == rets2[i].value);
        }
        tgt.addPre(refines.implies(ref_expr &&
                                   ub.implies(ub2) &&
                                   mem_state.implies(mem_state2)));
      }
    }
  }
}

expr State::simplifyWithAxioms(expr &&e) const {
  return axioms.contains(e) ? expr(true) : move(e);
}

}
