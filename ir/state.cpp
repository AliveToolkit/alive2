// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/function.h"
#include "ir/globals.h"
#include "smt/smt.h"
#include "util/errors.h"
#include <cassert>

using namespace smt;
using namespace util;
using namespace std;

namespace IR {

expr State::CurrentDomain::operator()(bool noret) const {
  return path && UB() && (noret ? noreturn() : expr(true));
}

State::CurrentDomain::operator bool() const {
  return !path.isFalse() && UB && noreturn;
}

void State::CurrentDomain::reset() {
  path = true;
  UB.reset();
  noreturn.reset();
  undef_vars.clear();
}

expr State::DomainPreds::operator()() const {
  return path() && *UB();
}


State::State(Function &f, bool source)
  : f(f), source(source), memory(*this),
    return_val(f.getType().getDummyValue(false)), return_memory(memory) {}

void State::resetGlobals() {
  Memory::resetBids(1);
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
  return I == pres.end() ? nullptr : &I->second.first.path;
}

bool State::isUndef(const expr &e) const {
  return undef_vars.count(e) != 0;
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  ENSURE(seen_bbs.emplace(&bb).second);
  current_bb = &bb;

  domain.reset();

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
    auto &[dom, mem] = data;
    path.add(dom.path);
    expr p = dom.path();
    UB.add_disj(dom.UB, p);
    in_memory.add_disj(mem, move(p));
    domain.undef_vars.insert(dom.undef_vars.begin(), dom.undef_vars.end());
  }

  domain.path = path();
  domain.UB.add(*UB());
  memory = *in_memory();

  return domain;
}

void State::addJump(const BasicBlock &dst0, expr &&cond) {
  if (cond.isFalse())
    return;

  auto dst = &dst0;
  if (seen_bbs.count(dst)) {
    dst = &f.getBB("#sink");
  }

  cond &= domain.path;
  auto &data = predecessor_data[dst][current_bb];
  data.second.add(memory, cond);
  data.first.UB.add(domain.UB(), cond);
  data.first.path.add(move(cond) && domain.noreturn());
  data.first.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  data.first.undef_vars.insert(domain.undef_vars.begin(),
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
  return_val.add(val, domain.path);
  return_memory.add(memory, domain.path);
  return_domain.add(domain(true));
  return_ub.add(domain(false));
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  undef_vars.clear();
  addUB(expr(false));
}

void State::addUB(expr &&ub) {
  domain.UB.add(domain.noreturn().implies(move(ub)));
  if (!ub.isConst())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(const expr &ub) {
  domain.UB.add(domain.noreturn().implies(ub));
  if (!ub.isConst())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(AndExpr &&ubs) {
  auto noret = domain.noreturn();
  for (auto &ub : ubs)
    domain.UB.add(noret.implies(ub));
  domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addNoReturn(expr &&noreturn) {
  domain.noreturn.add(move(noreturn));
  domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

const vector<StateValue>
State::addFnCall(const string &name, vector<StateValue> &&inputs,
                 vector<pair<StateValue, bool>> &&ptr_inputs,
                 const vector<Type*> &out_types, bool reads_memory,
                 bool writes_memory, bool argmemonly,
                 vector<StateValue> &&returned_val) {
  // TODO: handle changes to memory due to fn call
  // TODO: can read/write=false fn calls be removed?

  expr all_args_np(true);
  bool all_valid = true;
  for (auto &v : inputs) {
    all_args_np &= v.non_poison;
    all_valid &= v.isValid();
  }
  for (auto &v : ptr_inputs) {
    all_args_np &= v.first.non_poison;
    all_valid &= v.first.isValid();
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
    if (!returned_val.empty())
      values = move(returned_val);
    else {
      string valname = name + "#val";
      string npname = name + "#np";
      for (auto t : out_types) {
        values.emplace_back(mk_val(*t, valname).first,
                            expr::mkFreshVar(npname.c_str(), false));
      }
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

  if (all_args_np.isTrue())
    return I->second.retvals;

  // if any of the arguments is poison, yield an arbitrary value, such that
  // f(poison) -> f(42) works
  vector<StateValue> ret;
  auto T = out_types.begin();
  auto var_name = name + "#pval";
  for (auto &[v, np] : I->second.retvals) {
    auto [val_poison, var] = mk_val(**T, var_name);
    ret.emplace_back(expr::mkIf(all_args_np, v, val_poison), expr(np));
    addQuantVar(move(var));
    ++T;
  }
  return ret;
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
    ret.add(data.first.path());
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
    for (auto I = data.begin(), E = data.end(); I != E; ++I) {
      auto &[ins, ptr_ins, mem, reads, argmem] = I->first;
      auto &[rets, ub, mem_state, used] = I->second;
      assert(used); (void)used;

      auto &data2 = tgt.fn_call_data.at(fn);
      for (auto I2 = data2.begin(), E2 = data2.end(); I2 != E2; ++I2) {
        auto &[ins2, ptr_ins2, mem2, reads2, argmem2] = I2->first;
        auto &[rets2, ub2, mem_state2, used2] = I2->second;

        if (!used2 || reads != reads2 || argmem != argmem2)
          continue;

        expr refines(true), is_val_eq(true);
        for (unsigned i = 0, e = ins.size(); i != e; ++i) {
          expr eq_val = ins[i].value == ins2[i].value;
          is_val_eq &= eq_val;
          refines &= ins[i].non_poison.implies(eq_val && ins2[i].non_poison);
        }

        if (is_val_eq.isFalse() && refines.isFalse())
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
                      .fninput_refined(Pointer(mem2, ptr_in2.value), is_byval2);
          is_val_eq &= eq_val;
          refines &= ptr_in.non_poison
                       .implies(eq_val && ptr_in2.non_poison);
        }

        if (reads2) {
          auto restrict_ptrs = argmem2 ? &ptr_ins2 : nullptr;
          expr mem_refined = mem.refined(mem2, true, restrict_ptrs).first;
          is_val_eq &= mem_refined;
          refines &= mem_refined;
        }

        expr ref_expr(true), eq_expr(true);
        for (unsigned i = 0, e = rets.size(); i != e; ++i) {
          eq_expr &= rets[i].value == rets2[i].value;
          ref_expr &= rets[i].non_poison.implies(rets2[i].non_poison);
        }
        tgt.addPre(is_val_eq.implies(eq_expr));
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
