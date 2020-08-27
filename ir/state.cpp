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

void State::ValueAnalysis::intersect(const State::ValueAnalysis &other) {
  set<const Value*> results;
  set_intersection(non_poison_vals.begin(), non_poison_vals.end(),
      other.non_poison_vals.begin(), other.non_poison_vals.end(),
      inserter(results, results.begin()));
  non_poison_vals = move(results);
}

void State::ValueAnalysis::reset() {
  non_poison_vals.clear();
}


State::State(Function &f, bool source)
  : f(f), source(source), memory(*this),
    return_val(f.getType().getDummyValue(false)), return_memory(memory) {}

void State::resetGlobals() {
  Memory::resetGlobals();
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

  auto simplify = [&](StateValue& sv, bool use_new_slot) -> StateValue& {
    if (analysis.non_poison_vals.count(&val)) {
      // Store a simplified state value into tmp_values
      if (use_new_slot) {
        assert(i_tmp_values < tmp_values.size());
        tmp_values[i_tmp_values++] = sv;
      } else {
        assert(i_tmp_values > 0);
        tmp_values[i_tmp_values - 1] = sv;
      }
      StateValue &sv_new = tmp_values[i_tmp_values - 1];
      expr np = sv_new.non_poison;
      sv_new.non_poison = np.isBool() ? true : expr::mkUInt(-1, np.bits());
      return sv_new;
    }
    return sv;
  };

  if (uvars.empty() || !used || disable_undef_rewrite) {
    used = true;
    undef_vars.insert(uvars.begin(), uvars.end());
    return simplify(sval, true);
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

  assert(i_tmp_values < tmp_values.size());
  tmp_values[i_tmp_values++] = move(sval_new);
  return simplify(tmp_values[i_tmp_values - 1], false);
}

const StateValue& State::getAndAddUndefs(const Value &val) {
  auto &v = (*this)[val];
  for (auto uvar: at(val).second)
    addQuantVar(move(uvar));
  return v;
}

const StateValue& State::getAndAddPoisonUB(const Value &val) {
  auto &v = (*this)[val];
  if (!v.non_poison.isTrue()) {
    analysis.non_poison_vals.insert(&val);
    if (v.non_poison.isBool())
      addUB(v.non_poison);
    else
      // UB if any element is poison
      addUB(v.non_poison != expr::mkUInt(-1, v.non_poison.bits()));
  }
  return v;
}

const State::ValTy& State::at(const Value &val) const {
  return get<1>(values[values_map.at(&val)]);
}

const OrExpr* State::jumpCondFrom(const BasicBlock &bb) const {
  auto &pres = predecessor_data.at(current_bb);
  auto I = pres.find(&bb);
  return I == pres.end() ? nullptr : &I->second.domain.path;
}

bool State::isUndef(const expr &e) const {
  return undef_vars.count(e) != 0;
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  ENSURE(seen_bbs.emplace(&bb).second);
  current_bb = &bb;

  domain.reset();
  analysis.reset();

  if (&f.getFirstBB() == &bb)
    return true;

  auto I = predecessor_data.find(&bb);
  if (I == predecessor_data.end())
    return false;

  DisjointExpr<Memory> in_memory;
  DisjointExpr<expr> UB;
  OrExpr path;

  bool isFirst = true;
  for (auto &[src, data] : I->second) {
    (void)src;
    auto &[dom, anlys, mem] = data;
    path.add(dom.path);
    expr p = dom.path();
    UB.add_disj(dom.UB, p);
    in_memory.add_disj(mem, move(p));
    domain.undef_vars.insert(dom.undef_vars.begin(), dom.undef_vars.end());

    if (isFirst)
      analysis = anlys;
    else
      analysis.intersect(anlys);
    isFirst = false;
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
  data.mem.add(memory, cond);
  data.domain.UB.add(domain.UB(), cond);
  data.domain.path.add(move(cond));
  data.domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  data.domain.undef_vars.insert(domain.undef_vars.begin(),
                                domain.undef_vars.end());
  data.analysis = analysis;
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

void State::addReturn(StateValue &&val) {
  return_val.add(move(val), domain.path);
  return_memory.add(memory, domain.path);
  return_domain.add(domain());
  function_domain.add(domain());
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  undef_vars.clear();
  addUB(expr(false));
}

void State::addUB(expr &&ub) {
  bool isconst = ub.isConst();
  domain.UB.add(move(ub));
  if (!isconst)
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(const expr &ub) {
  domain.UB.add(ub);
  if (!ub.isConst())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(AndExpr &&ubs) {
  bool isconst = ubs.isTrue();
  domain.UB.add(move(ubs));
  if (!isconst)
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
}

void State::addNoReturn() {
  return_memory.add(memory, domain.path);
  function_domain.add(domain());
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  undef_vars.clear();
  addUB(expr(false));
}

vector<StateValue>
State::addFnCall(const string &name, vector<StateValue> &&inputs,
                 vector<Memory::PtrInput> &&ptr_inputs,
                 const vector<Type*> &out_types, const FnAttrs &attrs) {
  // TODO: can read/write=false fn calls be removed?

  bool reads_memory = !attrs.has(FnAttrs::NoRead);
  bool writes_memory = !attrs.has(FnAttrs::NoWrite);
  bool argmemonly = attrs.has(FnAttrs::ArgMemOnly);
  bool noundef = attrs.has(FnAttrs::NoUndef);

  bool all_valid = std::all_of(inputs.begin(), inputs.end(),
                                [](auto &v) { return v.isValid(); }) &&
                   std::all_of(ptr_inputs.begin(), ptr_inputs.end(),
                                [](auto &v) { return v.val.isValid(); });

  if (!all_valid) {
    addUB(expr());
    return vector<StateValue>(out_types.size());
  }

  for (auto &v : ptr_inputs) {
    if (!v.byval && !v.nocapture && !v.val.non_poison.isFalse())
      memory.escapeLocalPtr(v.val.value);
  }

  // TODO: this doesn't need to compare the full memory, just a subset of fields
  auto call_data_pair
    = fn_call_data[name].try_emplace({ move(inputs), move(ptr_inputs),
                                       memory, reads_memory, argmemonly });
  auto &I = call_data_pair.first;
  bool inserted = call_data_pair.second;

  if (inserted) {
    auto mk_val = [&](const Type &t, const string &name) {
      if (t.isPtrType())
        return memory.mkFnRet(name.c_str(), I->first.args_ptr).first;

      return expr::mkFreshVar(name.c_str(), t.getDummyValue(false).value);
    };

    vector<StateValue> values;
    string valname = name + "#val";
    string npname = name + "#np";
    for (auto t : out_types) {
      expr np = noundef ? expr(true) : expr::mkFreshVar(npname.c_str(), false);
      values.emplace_back(mk_val(*t, valname), move(np));
    }

    string ub_name = name + "#ub";
    I->second
      = { move(values), expr::mkFreshVar(ub_name.c_str(), false),
          writes_memory
            ? memory.mkCallState(argmemonly ? &I->first.args_ptr : nullptr,
                               attrs.has(FnAttrs::NoFree))
            : Memory::CallState(),
          true };
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
    ret.add(data.domain.path());
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

  memory.syncWithSrc(src.returnMemory());
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

        set<expr> undef_vars;
        for (unsigned i = 0, e = ptr_ins.size(); i != e; ++i) {
          // TODO: needs to take read/read2 as input to control if mem blocks
          // need to be compared
          auto &[ptr_in, is_byval, is_nocapture] = ptr_ins[i];
          auto &[ptr_in2, is_byval2, is_nocapture2] = ptr_ins2[i];
          (void)is_nocapture;
          (void)is_nocapture2;
          if (!is_byval && is_byval2) {
            // byval is added at target; this is not supported yet.
            refines = false;
            break;
          }
          expr eq_val = Pointer(mem, ptr_in.value)
                          .fninputRefined(Pointer(mem2, ptr_in2.value),
                                          undef_vars, is_byval2);
          refines &= ptr_in.non_poison
                       .implies(eq_val && ptr_in2.non_poison);

          if (refines.isFalse())
            break;
        }

        if (refines.isFalse())
          continue;

        quantified_vars.insert(undef_vars.begin(), undef_vars.end());

        if (reads2) {
          auto restrict_ptrs = argmem2 ? &ptr_ins2 : nullptr;
          auto data = mem.refined(mem2, true, restrict_ptrs);
          refines &= get<0>(data);
          quantified_vars.insert(get<2>(data).begin(), get<2>(data).end());
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
