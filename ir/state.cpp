// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/function.h"
#include "smt/smt.h"
#include "util/errors.h"
#include <cassert>

using namespace smt;
using namespace util;
using namespace std;

namespace IR {

State::State(const Initializers &inits, const Function &f, bool source)
  : inits(inits), f(f), source(source), memory(*this, f.isLittleEndian()),
    return_domain(false), return_val(f.getType().getDummyValue(false)),
    return_memory(memory) {}

void State::resetGlobals() {
  Memory::resetGlobalData();
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

const State::ValTy& State::at(const Value &val) const {
  return get<1>(values[values_map.at(&val)]);
}

const expr* State::jumpCondFrom(const BasicBlock &bb) const {
  auto &pres = predecessor_data.at(current_bb);
  auto I = pres.find(&bb);
  return I == pres.end() ? nullptr : &I->second.first.first;
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  ENSURE(seen_bbs.emplace(&bb).second);
  current_bb = &bb;

  if (&f.getFirstBB() == &bb) {
    domain.first = true;
    return true;
  }

  auto I = predecessor_data.find(&bb);
  if (I == predecessor_data.end())
    return false;

  domain.first = false;
  domain.second.clear();

  DisjointExpr<Memory> in_memory;

  for (auto &[src, data] : I->second) {
    (void)src;
    auto &[dom, mem] = data;
    auto &[cond, vars] = dom;
    domain.first |= cond;
    domain.second.insert(vars.begin(), vars.end());
    in_memory.add(mem, cond);
  }

  if (domain.first.isFalse())
    return false;

  memory = in_memory();

  return true;
}

void State::addJump(const BasicBlock &dst, expr &&cond) {
  if (seen_bbs.count(&dst))
    throw AliveException("Loops are not supported yet! Skipping function.",
                         false);

  cond &= domain.first;
  auto p = predecessor_data[&dst].try_emplace(current_bb, DomainTy({}, {}),
                                              memory);
  if (!p.second) {
    p.first->second.second = Memory::mkIf(cond, memory, p.first->second.second);
    p.first->second.first.first |= move(cond);
    p.first->second.first.second.insert(undef_vars.begin(), undef_vars.end());
  } else {
    p.first->second.first.first  = move(cond);
    p.first->second.first.second = undef_vars;
  }
  p.first->second.first.second.insert(domain.second.begin(),
                                      domain.second.end());
}

void State::addJump(const BasicBlock &dst) {
  addJump(dst, true);
  domain.first = false;
}

void State::addJump(StateValue &&cond, const BasicBlock &dst) {
  addJump(dst, move(cond.value) && move(cond.non_poison));
}

void State::addCondJump(const StateValue &cond, const BasicBlock &dst_true,
                        const BasicBlock &dst_false) {
  addJump(dst_true,  cond.value == 1 && cond.non_poison);
  addJump(dst_false, cond.value == 0 && cond.non_poison);
  domain.first = false;
}

void State::addReturn(const StateValue &val) {
  return_val.add(val, domain.first);
  return_memory.add(memory, domain.first);
  return_domain |= domain.first;
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.second.begin(), domain.second.end());
  undef_vars.clear();
  domain.first = false;
}

void State::addUB(expr &&ub) {
  domain.first &= move(ub);
  domain.second.insert(undef_vars.begin(), undef_vars.end());
}

void State::addUB(const expr &ub) {
  domain.first &= ub;
  domain.second.insert(undef_vars.begin(), undef_vars.end());
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

void State::addGlobalVarBid(const string &glbvar, unsigned bid) {
  ENSURE(glbvar_bids.emplace(glbvar, bid).second);
}

bool State::hasGlobalVarBid(const string &glbvar, unsigned &bid) const {
  auto itr = glbvar_bids.find(glbvar);
  bool found = itr != glbvar_bids.end();
  if (found) {
    bid = itr->second;
  }
  return found;
}

void State::copyGlobalVarBidsFromSrc(const State &src) {
  assert(glbvar_bids.empty());
  assert(src.isSource());
  glbvar_bids = src.glbvar_bids;

  Memory::resetLocalBids();
}

}
