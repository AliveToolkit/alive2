// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/function.h"
#include "ir/globals.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "util/config.h"
#include "util/errors.h"
#include <algorithm>
#include <cassert>

using namespace smt;
using namespace util;
using namespace std;

static void throw_oom_exception() {
  throw AliveException("Out of memory; skipping function.", false);
}

namespace IR {

SMTMemoryAccess::SMTMemoryAccess(const MemoryAccess &val)
  : val(expr::mkUInt(val.val, 2 * AccessType::NumTypes)) {}

// format ..rw..
expr SMTMemoryAccess::canAccess(AccessType ty) const {
  unsigned bit = ty * 2;
  return val.extract(bit + 1, bit) == 3;
}

expr SMTMemoryAccess::canRead(AccessType ty) const {
  unsigned bit = ty * 2 + 1;
  return val.extract(bit, bit) == 1;
}

expr SMTMemoryAccess::canWrite(AccessType ty) const {
  unsigned bit = ty * 2;
  return val.extract(bit, bit) == 1;
}

expr SMTMemoryAccess::canOnlyRead(AccessType ty) const {
  AndExpr ret;
  for (unsigned i = 0; i < AccessType::NumTypes; ++i) {
    ret.add(canRead(AccessType(i)) == expr(i == ty));
  }
  return ret();
}

expr SMTMemoryAccess::canOnlyWrite(AccessType ty) const {
  AndExpr ret;
  for (unsigned i = 0; i < AccessType::NumTypes; ++i) {
    ret.add(canWrite(AccessType(i)) == expr(i == ty));
  }
  return ret();
}

expr SMTMemoryAccess::canReadSomething() const {
  OrExpr ret;
  for (unsigned i = 0; i < AccessType::NumTypes; ++i) {
    ret.add(canRead(AccessType(i)));
  }
  return ret();
}

expr SMTMemoryAccess::canWriteSomething() const {
  OrExpr ret;
  for (unsigned i = 0; i < AccessType::NumTypes; ++i) {
    ret.add(canWrite(AccessType(i)));
  }
  return ret();
}

expr SMTMemoryAccess::refinedBy(const SMTMemoryAccess &other) const {
  return (val & other.val) == val;
}

SMTMemoryAccess
SMTMemoryAccess::SMTMemoryAccess::mkIf(const expr &cond,
                                       const SMTMemoryAccess &then,
                                       const SMTMemoryAccess &els) {
  return expr::mkIf(cond, then.val, els.val);
}


expr State::CurrentDomain::operator()() const {
  return path && UB();
}

State::CurrentDomain::operator bool() const {
  return !path.isFalse() && UB;
}

template<class T>
static T intersect_set(const T &a, const T &b) {
  T results;
  ranges::set_intersection(a, b, inserter(results, results.begin()), less{});
  return results;
}

void State::ValueAnalysis::meet_with(const State::ValueAnalysis &other) {
  non_poison_vals = intersect_set(non_poison_vals, other.non_poison_vals);
  non_undef_vals = intersect_set(non_undef_vals, other.non_undef_vals);
  unused_vars = intersect_set(unused_vars, other.unused_vars);
  ranges_fn_calls.meet_with(other.ranges_fn_calls);
}

void State::ValueAnalysis::clear_smt() {
  non_poison_vals.clear();
  non_undef_vals = decltype(non_undef_vals)();
  unused_vars = decltype(unused_vars)();
}

void State::ValueAnalysis::FnCallRanges::inc(const string &name,
                                             const SMTMemoryAccess &access) {
  bool canwrite = !access.canWriteSomething().isFalse();

  auto [I, inserted] = try_emplace(name);
  if (inserted) {
    I->second.first.emplace(1, canwrite);
    I->second.second = access;
  } else {
    set<pair<unsigned, bool>> new_set;
    for (auto [n, writes0] : I->second.first) {
      new_set.emplace(n+1, writes0 | canwrite);
    }
    I->second.first   = std::move(new_set);
    I->second.second |= access;
  }
}

bool
State::ValueAnalysis::FnCallRanges::overlaps(const string &callee,
                                             const SMTMemoryAccess &call_access,
                                             const FnCallRanges &other) const {
  if (call_access.canReadSomething().isFalse())
    return true;

  auto skip
    = [call_access, &callee](const auto &fn, const SMTMemoryAccess &access) {
    if (access.canOnlyWrite(MemoryAccess::Inaccessible).isTrue()) {
      // If this fn can only write to inaccessible memory, ignore if it's not
      // our callee as callee can't read from that memory
      if (fn != callee)
        return true;

      // If the trace only writes to inaccessible memory, but the callee can't
      // read it, any mismatch in the number of calls is irrelevant
      if (call_access.canRead(MemoryAccess::Inaccessible).isFalse())
        return true;
    }

    if (call_access.canOnlyRead(MemoryAccess::Inaccessible).isTrue() &&
        fn == callee) {
      if (access.canWrite(MemoryAccess::Inaccessible).isFalse())
        return true;
    }

    // These may be calls that take allocas as arguments that aren't read by
    // anyone else
    if (access.canOnlyWrite(MemoryAccess::Args).isTrue())
      return true;

    return false;
  };

  for (auto &[fn, pair] : *this) {
    auto &[calls, access] = pair;

    if (skip(fn, access))
      continue;

    auto I = other.find(fn);
    if (I == other.end()) {
      if (calls.count({0, true}))
        continue;
      return false;
    }

    // A function that doesn't read anything, may write the same thing on
    // every call. So a mismatch in the number of calls must be ignored.
    if ((access | I->second.second).canReadSomething().isFalse())
      continue;

    auto set = intersect_set(calls, I->second.first);
    // must only have write accesses
    assert(ranges::all_of(set, [](auto &p) { return p.second; }));
    if (set.empty())
      return false;
  }

  for (auto &[fn, pair] : other) {
    auto &[calls, access] = pair;
    if (skip(fn, access))
      continue;

    if (!calls.count({0, true}) && !count(fn))
      return false;
  }

  return true;
}

bool State::ValueAnalysis::FnCallRanges::isLargerThanInclReads(
  const FnCallRanges &other) const {
  for (auto &[fn, pair] : *this) {
    auto &[calls, access] = pair;
    auto I = other.find(fn);
    if (I == other.end())
      continue;

    auto first_val = calls.begin()->first;
    auto other_last_val = I->second.first.rbegin()->first;
    if (first_val < other_last_val)
      return false;
  }

  for (auto &[fn, pair] : other) {
    if (!count(fn))
      return false;
  }
  return true;
}

State::ValueAnalysis::FnCallRanges
State::ValueAnalysis::FnCallRanges::project(const string &name) const {
  auto I = find(name);
  if (I == end())
    return {};
  FnCallRanges ranges;
  ranges.emplace(name, I->second);
  return ranges;
}

void State::ValueAnalysis::FnCallRanges::keep_only_writes() {
  for (auto I = begin(); I != end(); ) {
    auto &[calls, access] = I->second;
    for (auto II = calls.begin(); II != calls.end(); ) {
      if (!II->second)
        II = calls.erase(II);
      else
        ++II;
    }
    if (calls.empty())
      I = erase(I);
    else
      ++I;
  }
}

void State::ValueAnalysis::FnCallRanges::meet_with(const FnCallRanges &other) {
  for (auto &[fn, pair] : other) {
    auto &[calls, access] = pair;
    auto [I, inserted] = try_emplace(fn, pair);
    if (inserted) {
      I->second.first.emplace(0, true);
    } else {
      I->second.first.insert(calls.begin(), calls.end());
      I->second.second |= access;
    }
  }

  for (auto &[fn, pair] : *this) {
    auto &[calls, access] = pair;
    if (!other.count(fn))
      calls.emplace(0, true);
  }
}

State::VarArgsData
State::VarArgsData::mkIf(const expr &cond, VarArgsData &&then,
                         VarArgsData &&els) {
  VarArgsData ret;
  for (auto &[ptr, entry] : then.data) {
    auto other = els.data.find(ptr);
    if (other == els.data.end()) {
      ret.data.try_emplace(ptr, cond && entry.alive, std::move(entry.next_arg),
                           std::move(entry.num_args),
                           std::move(entry.is_va_start),
                           std::move(entry.active));
    } else {
#define C(f) expr::mkIf(cond, entry.f, other->second.f)
      ret.data.try_emplace(ptr, C(alive), C(next_arg), C(num_args),
                           C(is_va_start), C(active));
#undef C
    }
  }

  for (auto &[ptr, entry] : els.data) {
    if (then.data.count(ptr))
      continue;
    ret.data.try_emplace(ptr, !cond && entry.alive, std::move(entry.next_arg),
                         std::move(entry.num_args),
                         std::move(entry.is_va_start), std::move(entry.active));
  }

  return ret;
}

State::State(const Function &f, bool source)
  : f(f), source(source), memory(*this),
    fp_rounding_mode(expr::mkVar("fp_rounding_mode", 3)),
    fp_denormal_mode(expr::mkVar("fp_denormal_mode", 2)),
    return_val(DisjointExpr(f.getType().getDummyValue(false))) {
  predecessor_data.reserve(f.getNumBBs());
}

void State::resetGlobals() {
  Memory::resetGlobals();
}

const State::ValTy& State::exec(const Value &v) {
  assert(undef_vars.empty());
  domain.noreturn = true;
  auto val = v.toSMT(*this);

  auto value_ub = domain.UB;
  if (config::disallow_ub_exploitation)
    value_ub.add(!guardable_ub());

  auto [I, inserted]
    = values.try_emplace(&v, ValTy{std::move(val), domain.noreturn,
                                   std::move(value_ub), std::move(undef_vars)});
  assert(inserted);

  // As an optimization, record that this value has not yet been used, so
  // we can use this undef variable (if any) on the first use
  // This saves one rewrite per definition
  // We cannot do this optimization in ASM mode because an undef value may
  // trigger a poison value in the source, but since the target does not have
  // poison values, it must be converted into a non-det value that must be
  // able to range over the full domain, not just the non-poison domain.
  if (!config::tgt_is_asm)
    analysis.unused_vars.insert(&v);

  // cleanup potentially used temporary values due to undef rewriting
  while (i_tmp_values > 0) {
    tmp_values[--i_tmp_values] = StateValue();
  }

  return I->second;
}

static expr eq_except_padding(const Memory &m, const Type &ty, const expr &e1,
                              const expr &e2, bool ptr_compare) {
  if (ptr_compare && ty.isPtrType())
    return Pointer(m, e1) == Pointer(m, e2);

  const auto *aty = ty.getAsAggregateType();
  if (!aty)
    return e1 == e2;

  StateValue sv1{expr(e1), expr()};
  StateValue sv2{expr(e2), expr()};
  expr result = true;

  for (unsigned i = 0; i < aty->numElementsConst(); ++i) {
    if (aty->isPadding(i))
      continue;

    result &= eq_except_padding(m, aty->getChild(i), aty->extract(sv1, i).value,
                                aty->extract(sv2, i).value, ptr_compare);
  }
  return result;
}

expr State::strip_undef_and_add_ub(const Value &val, const expr &e,
                                   bool ptr_compare) {
  if (undef_vars.empty() || e.isConst())
    return e;

  if (isUndef(e)) {
    addUB(expr(false));
    return expr::mkNumber("0", e);
  }

  auto vars = e.vars();
  auto undef_vars = intersect_set(vars, this->undef_vars);
  if (undef_vars.empty())
    return e;

  // check if any var is already known to be non-undef
  vector<pair<expr,expr>> repls;
  set<expr> missing_tests;
  expr conds = true;
  for (auto &var : vars) {
    if (var.fn_name().starts_with("isundef_")) {
      expr test = var == 0;
      if (domain.UB.contains(test)) {
        conds &= test;
        repls.emplace_back(std::move(test), true);
      } else {
        missing_tests.emplace(var);
      }
    }
  }

  // some sort of undef concatenated with something else
  if (repls.empty() && missing_tests.empty()) {
    addUB(expr(false));
    return expr::mkNumber("0", e);
  }

  if (missing_tests.empty())
    return e.subst_simplify(repls);

  expr e2;
  {
    auto repls2 = repls;
    for (auto &u : undef_vars) {
      repls2.emplace_back(u, expr::mkFreshVar("undef", u));
    }
    e2 = e.subst(repls2);
  }

  set<expr> qvars;
  for (auto &var : vars) {
    auto name = var.fn_name();
    if (name.starts_with("isundef_") ||
        name.starts_with("undef!") ||
        var.isQVar())
      continue;
    qvars.emplace(var);
  }

  Solver s;
  s.add(conds);
  s.add(expr::mkForAll(qvars, !eq_except_padding(getMemory(), val.getType(), e,
                                                 e2, ptr_compare)));
  bool all_decided = true;

  // check each undef var in turn by making all other vars non-undef
  // if the expressions yields a different value, then the selected var can't
  // be undef
  for (auto &var : missing_tests) {
    SolverPush push(s);
    for (auto &miss_var : missing_tests) {
      expr test = miss_var == 0;
      if (miss_var.eq(var)) {
        test = !test;
      }
      s.add(test);
    }
    auto res = s.check("non-undef inference", true);
    expr test = var == 0;
    if (res.isSat()) { // var can't be undef
      addUB(test);
      auto var_name = var.fn_name().substr(sizeof("isundef_")-1);
      // mark the var as non-undef for future uses
      for (auto &[v, val] : values) {
        if (v->getName() == var_name) {
          analysis.non_undef_vals
                  .emplace(v, val.val.value.subst(test, true).simplify());
          break;
        }
      }
      repls.emplace_back(std::move(test), true);
    } else {
      // can't conclude anything
      all_decided = false;
      break;
    }
  }

  if (all_decided) {
    e2 = e.subst_simplify(repls);
    auto vars = e2.vars();
    // if there are still undef variables (not originating from inputs),
    // we need to account for the extra conditions that make it non-undef
    auto I = ranges::find_if(vars, [&](auto &var) { return isUndef(var); });
    if (I == vars.end())
      return e2;
  }

  // check if original expression is equal to an expression where undefs are
  // fixed to a const value
  auto repls2 = repls;
  for (auto &undef : undef_vars) {
    expr newv = expr::mkFreshVar("#undef'", undef);
    addQuantVar(newv);
    repls2.emplace_back(undef, std::move(newv));
  }
  addUB(eq_except_padding(getMemory(), val.getType(), e, e.subst(repls2),
                          ptr_compare));
  return e.subst_simplify(repls);
}

void State::check_enough_tmp_slots() {
  if (i_tmp_values >= tmp_values.size())
    throw AliveException("Too many temporaries", false);
}

const StateValue& State::eval(const Value &val, bool quantify_nondet) {
  auto &[sval, _retdom, _ub, uvars] = values.at(&val);

  auto undef_itr = analysis.non_undef_vals.find(&val);
  bool is_non_undef = undef_itr != analysis.non_undef_vals.end();
  bool is_non_poison = analysis.non_poison_vals.count(&val);

  auto simplify = [&](StateValue &sv0, bool use_new_slot) -> StateValue& {
    if (!is_non_undef && !is_non_poison)
      return sv0;

    if (use_new_slot) {
      check_enough_tmp_slots();
      tmp_values[i_tmp_values++] = sv0;
    }
    assert(i_tmp_values > 0);
    StateValue &sv_new = tmp_values[i_tmp_values - 1];
    if (is_non_undef) {
      sv_new.value = undef_itr->second;
    }
    if (is_non_poison) {
      const expr &np = sv_new.non_poison;
      sv_new.non_poison = np.isBool() ? true : expr::mkInt(-1, np);
    }
    return sv_new;
  };

  if (is_non_undef) {
    // We don't need to add uvar to undef_vars
    quantified_vars.insert(uvars.begin(), uvars.end());
    return simplify(sval, true);
  }

  auto unused_itr = analysis.unused_vars.find(&val);
  bool unused = unused_itr != analysis.unused_vars.end();
  if (uvars.empty() || unused || disable_undef_rewrite) {
    if (unused)
      analysis.unused_vars.erase(unused_itr);
    undef_vars.insert(uvars.begin(), uvars.end());
    return simplify(sval, true);
  }

  vector<pair<expr, expr>> repls;
  for (auto &u : uvars) {
    repls.emplace_back(u, expr::mkFreshVar("undef", u));
  }

  if (hit_half_memory_limit())
    throw_oom_exception();

  unsigned undef_repls = repls.size();
  assert(undef_repls > 0);

  if (quantify_nondet) {
    for (auto &var : sval.vars()) {
      if (nondet_vars.count(var))
        repls.emplace_back(var, getFreshNondetVar("nondetvar", var));
    }
  }

  auto sval_new = sval.subst(repls);
  if (sval_new.eq(sval)) {
    uvars.clear();
    return simplify(sval, true);
  }

  unsigned i = 0;
  for (auto &p : repls) {
    undef_vars.emplace(std::move(p.second));
    if (++i == undef_repls)
      break;
  }

  check_enough_tmp_slots();

  tmp_values[i_tmp_values++] = std::move(sval_new);
  return simplify(tmp_values[i_tmp_values - 1], false);
}

const StateValue& State::getAndAddUndefs(const Value &val) {
  auto &v = (*this)[val];
  for (auto uvar: at(val)->undef_vars)
    addQuantVar(std::move(uvar));
  return v;
}

static expr not_poison_except_padding(const Type &ty, const expr &np) {
  const auto *aty = ty.getAsAggregateType();
  if (!aty) {
    assert(!np.isValid() || np.isBool());
    return np;
  }

  StateValue sv{expr(), expr(np)};
  expr result = true;

  for (unsigned i = 0; i < aty->numElementsConst(); ++i) {
    if (aty->isPadding(i))
      continue;

    result &= not_poison_except_padding(aty->getChild(i),
                                        aty->extract(sv, i).non_poison);
  }
  return result;
}

const StateValue&
State::getAndAddPoisonUB(const Value &val, bool undef_ub_too,
                         bool ptr_compare) {
  auto &sv = (*this)[val];

  bool poison_already_added = !analysis.non_poison_vals.insert(&val).second;
  if (poison_already_added && !undef_ub_too)
    return sv;

  expr v = sv.value;

  if (undef_ub_too) {
    auto [I, inserted] = analysis.non_undef_vals.try_emplace(&val);
    if (inserted) {
      I->second = strip_undef_and_add_ub(val, v, ptr_compare);
    }
    v = I->second;
  }

  if (!poison_already_added) {
    // mark all operands of val as non-poison if they propagate poison
    vector<Value*> todo;
    if (auto i = dynamic_cast<const Instr*>(&val)) {
      if (i->propagatesPoison())
        todo = i->operands();
    }
    while (!todo.empty()) {
      auto v = todo.back();
      todo.pop_back();
      if (!analysis.non_poison_vals.insert(v).second)
        continue;
      if (auto i = dynamic_cast<const Instr*>(v)) {
        if (i->propagatesPoison()) {
          auto ops = i->operands();
          todo.insert(todo.end(), ops.begin(), ops.end());
        }
      }
    }

    // If val is an aggregate, all elements should be non-poison
    addUB(not_poison_except_padding(val.getType(), sv.non_poison));
  }

  check_enough_tmp_slots();

  return tmp_values[i_tmp_values++] = { std::move(v),
           sv.non_poison.isBool() ? true : expr::mkInt(-1, sv.non_poison) };
}

const StateValue& State::getVal(const Value &val, bool is_poison_ub) {
  return is_poison_ub ? getAndAddPoisonUB(val) : (*this)[val];
}

const expr& State::getWellDefinedPtr(const Value &val) {
  return getAndAddPoisonUB(val, true, true).value;
}

const State::ValTy* State::at(const Value &val) const {
  auto I = values.find(&val);
  return I == values.end() ? nullptr : &I->second;
}

const OrExpr* State::jumpCondFrom(const BasicBlock &bb) const {
  auto &pres = predecessor_data.at(current_bb);
  auto I = pres.find(&bb);
  return I == pres.end() ? nullptr : &I->second.path;
}

bool State::isUndef(const expr &e) const {
  expr v;
  unsigned h, l;
  if (e.isExtract(v, h, l))
    return isUndef(v);
  return undef_vars.count(e) != 0;
}

bool State::isAsmMode() const {
  return getFn().has(FnAttrs::Asm);
}

expr State::getPath(BasicBlock &bb) const {
  if (&f.getFirstBB() == &bb)
    return true;
  
  auto I = predecessor_data.find(&bb);
  if (I == predecessor_data.end())
    return false; // Block is unreachable

  OrExpr path;
  for (auto &[src, data] : I->second) {
    path.add(data.path);
  }
  return std::move(path)();
}

void State::cleanup(const Value &val) {
  values.erase(&val);
  seen_bbs.clear();
  analysis.unused_vars.clear();
  analysis.non_poison_vals.clear();
  analysis.non_undef_vals.clear();
}

void State::cleanupPredecessorData() {
  predecessor_data.clear();
}

void State::copyUBFrom(const BasicBlock &bb) {
  if (config::disallow_ub_exploitation)
    return;

  // Time-travel UB: anything that happens before a possibly non-returning call
  // can be moved up to the entry of the BB.
  const Value *before_call = nullptr;
  for (auto &i : bb.instrs()) {
    if (auto *call = dynamic_cast<const FnCall*>(&i)) {
      if (!call->hasAttribute(FnAttrs::WillReturn))
        break;
    }
    before_call = &i;
  }
  if (!before_call)
    return;

  auto src_val_I = src_state->values.find(before_call);
  assert(src_val_I != src_state->values.end());
  domain.UB.add(src_val_I->second.domain);
}

void State::copyUBFromBB(
  const unordered_map<const BasicBlock*, BasicBlockInfo> &tgt_data) {
  auto I = src_bb_paths.find(domain.path);
  if (I == src_bb_paths.end())
    return;

  for (auto *src_bb : I->second) {
    bool all_paths_ok = true;
    for (auto &[_, src_data] : src_state->predecessor_data.at(src_bb)) {
      auto I = ranges::find_if(tgt_data, [&](const auto &p) {
        return is_eq(p.second.path <=> src_data.path);
      });
      if (I == tgt_data.end() ||
          !I->second.analysis.ranges_fn_calls.isLargerThanInclReads(
            src_data.analysis.ranges_fn_calls)) {
        all_paths_ok = false;
        break;
      }
    }
    if (all_paths_ok)
      copyUBFrom(*src_bb);
  }
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  ENSURE(seen_bbs.emplace(&bb).second);
  current_bb = &bb;

  if (&f.getFirstBB() == &bb) {
    if (src_state) {
      copyUBFromBB({});
      copyUBFrom(src_state->f.getFirstBB());
    }
    return true;
  }

  auto I = predecessor_data.find(&bb);
  if (I == predecessor_data.end())
    return false;

  if (hit_memory_limit())
    throw_oom_exception();

  DisjointExpr<Memory> in_memory;
  DisjointExpr<AndExpr> UB;
  DisjointExpr<VarArgsData> var_args_in;
  OrExpr path;

  domain.UB = AndExpr();

  bool isFirst = true;
  for (auto &[src, data] : I->second) {
    path.add(data.path);
    expr p = data.path();
    UB.add_disj(data.UB, p);

    // This data is never used again, so clean it up to reduce mem consumption
    in_memory.add_disj(std::move(data.mem), p);
    var_args_in.add(std::move(data.var_args), std::move(p));
    domain.undef_vars.insert(data.undef_vars.begin(), data.undef_vars.end());
    data.undef_vars.clear();

    if (isFirst)
      analysis = data.analysis;
    else
      analysis.meet_with(data.analysis);

    if (isSource())
      data.analysis.clear_smt();
    isFirst = false;
  }
  assert(!isFirst);

  domain.UB.add(std::move(UB).factor());
  domain.path   = std::move(path)();
  memory        = *std::move(in_memory)();
  var_args_data = *std::move(var_args_in)();

  if (src_state)
    copyUBFromBB(I->second);

  return domain;
}

void State::addJump(expr &&cond, const BasicBlock &dst0, bool always_jump) {
  always_jump = always_jump || cond.isTrue();

  cond &= domain.path;
  if (cond.isFalse() || !domain)
    return;

  auto dst = &dst0;
  if (seen_bbs.count(dst)) {
    dst = &f.getSinkBB();
  }

  auto &data = predecessor_data[dst][current_bb];
  if (always_jump) {
    data.mem.add(std::move(memory), cond);
    data.analysis = std::move(analysis);
    data.var_args = std::move(var_args_data);
  } else {
    data.mem.add(memory.dup(), cond);
    data.analysis = analysis;
    data.var_args = var_args_data;
  }
  data.UB.add(domain.UB, cond);
  data.path.add(std::move(cond));
  data.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  data.undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());

  if (always_jump)
    domain.path = false;
}

void State::addJump(const BasicBlock &dst) {
  addJump(true, dst, true);
}

void State::addCondJump(const expr &cond, const BasicBlock &dst_true,
                        const BasicBlock &dst_false) {
  expr cond_false = cond == 0;
  addJump(!cond_false, dst_true);
  addJump(std::move(cond_false), dst_false, true);
}

void State::addReturn(StateValue &&val) {
  get<0>(return_val).add(std::move(val), domain.path);
  get<0>(return_memory).add(std::move(memory), domain.path);
  auto dom = domain();
  return_domain.add(expr(dom));
  function_domain.add(std::move(dom));
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  undef_vars.clear();
  domain.path = false;
}

void State::addAxiom(AndExpr &&ands) {
  assert(ands);
  axioms.add(std::move(ands));
}

void State::addAxiom(expr &&axiom) {
  assert(!axiom.isFalse());
  axioms.add(std::move(axiom));
}

void State::addUB(pair<AndExpr, expr> &&ub) {
  addUB(std::move(ub.first));
  addGuardableUB(std::move(ub.second));
}

void State::addUB(expr &&ub) {
  if (!ub.isConst())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  domain.UB.add(std::move(ub));
}

void State::addUB(AndExpr &&ubs) {
  if (!ubs.isTrue())
    domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  domain.UB.add(std::move(ubs));
}

void State::addGuardableUB(expr &&ub) {
  if (config::disallow_ub_exploitation) {
    if (!ub.isConst())
      domain.undef_vars.insert(undef_vars.begin(), undef_vars.end());
    guardable_ub.add(domain.path && !ub);
  } else {
    addUB(std::move(ub));
  }
}

void State::addNoReturn(const expr &cond) {
  if (cond.isFalse())
    return;
  domain.noreturn = !cond;
  get<0>(return_memory).add(memory.dup(), domain.path && cond);
  function_domain.add(domain() && cond);
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  if (cond.isTrue())
    undef_vars.clear();
  addUB(!cond);
}

void State::addUnreachable() {
  unreachable_paths.add(domain());
}

expr State::FnCallInput::implies(const FnCallInput &rhs) const {
  if (noret != rhs.noret || willret != rhs.willret ||
      (rhs.memaccess.canReadSomething().isTrue() &&
        (fncall_ranges != rhs.fncall_ranges || is_neq(m <=> rhs.m))))
    return false;

  AndExpr eq;
  eq.add(memaccess.refinedBy(rhs.memaccess));
  for (unsigned i = 0, e = args_nonptr.size(); i != e; ++i) {
    eq.add(args_nonptr[i].implies(rhs.args_nonptr[i]));
  }

  for (unsigned i = 0, e = args_ptr.size(); i != e; ++i) {
    eq.add(args_ptr[i].implies(rhs.args_ptr[i]));
  }
  return eq();
}

expr State::FnCallInput::refinedBy(
  State &s, const string &callee, unsigned inaccessible_bid,
  const vector<StateValue> &args_nonptr2,
  const vector<PtrInput> &args_ptr2,
  const ValueAnalysis::FnCallRanges &fncall_ranges2,
  const Memory &m2, const SMTMemoryAccess &memaccess2, bool noret2,
  bool willret2) const {

  if (noret != noret2 ||
      willret != willret2 ||
      !fncall_ranges.overlaps(callee, memaccess2, fncall_ranges2))
    return false;

  AndExpr refines;
  refines.add(memaccess.refinedBy(memaccess2));

  assert(args_nonptr.size() == args_nonptr2.size());
  for (unsigned i = 0, e = args_nonptr.size(); i != e; ++i) {
    refines.add(args_nonptr[i].implies(args_nonptr2[i]));
  }

  if (!refines)
    return false;

  set<expr> undef_vars;
  assert(args_ptr.size() == args_ptr2.size());
  for (unsigned i = 0, e = args_ptr.size(); i != e; ++i) {
    auto &ptr1 = args_ptr[i];
    auto &ptr2 = args_ptr2[i];
    expr eq_val = Pointer(m, ptr1.val.value)
                    .fninputRefined(Pointer(m2, ptr2.val.value),
                                    undef_vars, ptr2.byval);
    refines.add(ptr1.val.non_poison.implies(ptr2.val.non_poison &&
                                            eq_val &&
                                            ptr1.implies_attrs(ptr2)));

    if (!refines)
      return false;
  }

  for (auto &v : undef_vars)
    s.addFnQuantVar(v);

  if (memaccess2.canReadSomething().isTrue()) {
    bool argmemonly = memaccess2.canOnlyRead(MemoryAccess::Args).isTrue();
    vector<PtrInput> dummy1, dummy2;
    auto restrict_ptrs = argmemonly ? &args_ptr : nullptr;
    auto restrict_ptrs2 = argmemonly ? &args_ptr2 : nullptr;
    if (memaccess2.canOnlyRead(MemoryAccess::Inaccessible).isTrue()) {
      assert(inaccessible_bid != -1u);
      dummy1.emplace_back(0,
        StateValue(Pointer(m, inaccessible_bid, false).release(), true), 0,
        false, false, false);
      dummy2.emplace_back(0,
        StateValue(Pointer(m2, inaccessible_bid, false).release(), true), 0,
        false, false, false);
      assert(!restrict_ptrs && !restrict_ptrs2);
      restrict_ptrs = &dummy1;
      restrict_ptrs2 = &dummy2;
    }
    auto data = m.refined(m2, true, restrict_ptrs, restrict_ptrs2);
    refines.add(get<0>(data));
    for (auto &v : get<2>(data))
      s.addFnQuantVar(v);
  }

  return refines();
}

State::FnCallOutput
State::FnCallOutput::replace(const StateValue &retval) const {
  FnCallOutput copy = *this;
  copy.retval = retval;
  return copy;
}

State::FnCallOutput State::FnCallOutput::mkIf(const expr &cond,
                                              const FnCallOutput &a,
                                              const FnCallOutput &b) {
  FnCallOutput ret;
  ret.retval    = StateValue::mkIf(cond, a.retval, b.retval);
  ret.ub        = expr::mkIf(cond, a.ub, b.ub);
  ret.noreturns = expr::mkIf(cond, a.noreturns, b.noreturns);
  ret.callstate = Memory::CallState::mkIf(cond, a.callstate, b.callstate);

  assert(a.ret_data.size() == b.ret_data.size());
  for (unsigned i = 0, e = a.ret_data.size(); i != e; ++i) {
    ret.ret_data.emplace_back(
      Memory::FnRetData::mkIf(cond, a.ret_data[i], b.ret_data[i]));
  }
  return ret;
}

expr State::FnCallOutput::implies(const FnCallOutput &rhs,
                                  const Type &retval_ty) const {
  expr ret = ub == rhs.ub;
  ret     &= noreturns == rhs.noreturns;
  ret     &= callstate == rhs.callstate;

  function<void(const StateValue&, const StateValue&, const Type&)> check_out
    = [&](const StateValue &a, const StateValue &b, const Type &ty) -> void {
    if (auto agg = ty.getAsAggregateType()) {
      for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
        if (!agg->isPadding(i))
          check_out(agg->extract(a, i), agg->extract(b, i), agg->getChild(i));
      }
      return;
    }
    ret &= a.implies(b);
  };
  check_out(retval, rhs.retval, retval_ty);
  return ret;
}

StateValue
State::addFnCall(const string &name, vector<StateValue> &&inputs,
                 vector<PtrInput> &&ptr_inputs,
                 const Type &out_type, StateValue &&ret_arg,
                 const Type *ret_arg_ty, vector<StateValue> &&ret_args,
                 const FnAttrs &attrs, unsigned indirect_call_hash) {
  bool noret   = attrs.has(FnAttrs::NoReturn);
  bool willret = attrs.has(FnAttrs::WillReturn);
  bool noundef = attrs.has(FnAttrs::NoUndef);
  bool noalias = attrs.has(FnAttrs::NoAlias);
  bool is_indirect = name.starts_with("#indirect_call");

  expr fn_ptr_bid;
  if (is_indirect) {
    assert(inputs.size() >= 1);
    fn_ptr_bid = inputs[0].value;
  }

  assert(!noret || !willret);

  bool all_valid
    = ranges::all_of(inputs, [](auto &v) { return v.isValid(); }) &&
      ranges::all_of(ptr_inputs, [](auto &v) { return v.val.isValid(); });

  if (!all_valid) {
    addUB(expr());
    return {};
  }

  auto isgvar = [&](const auto &decl) {
    if (auto gv = getFn().getGlobalVar(decl.name))
      return fn_ptr_bid == Pointer(memory, (*this)[*gv].value).getShortBid();
    return expr();
  };

  SMTMemoryAccess memaccess(attrs.mem);

  if (is_indirect) {
    DisjointExpr<SMTMemoryAccess> decl_access;

    // adjust attributes of pointer arguments
    for (auto &decl : getFn().getFnDecls()) {
      if (decl.hash() != indirect_call_hash)
        continue;

      auto cmp = isgvar(decl);
      if (!cmp.isValid())
        continue;

      decl_access.add(decl.attrs.mem, cmp);

      for (auto &ptr : ptr_inputs) {
        if (decl.inputs.size() != ret_args.size() ||
            (decl.is_varargs && ptr.idx < decl.inputs.size()))
          continue;
        auto &attrs = decl.inputs[ptr.idx].second;
        ptr.byval   = expr::mkIf(cmp, expr::mkUInt(attrs.blockSize, 64),
                                 ptr.byval);
        if (attrs.has(ParamAttrs::NoRead))
          ptr.noread |= cmp;
        if (attrs.has(ParamAttrs::NoWrite))
          ptr.nowrite |= cmp;
        if (attrs.has(ParamAttrs::NoCapture))
          ptr.nocapture |= cmp;
      }
    }

    memaccess &= *std::move(decl_access).mk(SMTMemoryAccess{
      expr::mkUF("#access_" + name, { fn_ptr_bid }, memaccess.val)});
  }

  if (!memaccess.canWrite(MemoryAccess::Args).isFalse() ||
      !memaccess.canWrite(MemoryAccess::Inaccessible).isFalse() ||
      !memaccess.canWrite(MemoryAccess::Other).isFalse()) {
    for (auto &v : ptr_inputs) {
      if (!(v.byval == 0).isFalse() && !v.nocapture.isTrue())
        memory.escapeLocalPtr(v.val.value, v.val.non_poison);
    }
  }

  StateValue retval;
  unsigned inaccessible_bid = -1u;
  if (!memaccess.canOnlyRead(MemoryAccess::Inaccessible).isFalse() ||
      !memaccess.canOnlyWrite(MemoryAccess::Inaccessible).isFalse())
    inaccessible_bid
      = inaccessiblemem_bids.try_emplace(name, inaccessiblemem_bids.size())
                            .first->second;

  ValueAnalysis::FnCallRanges call_ranges;
  if (!memaccess.canRead(MemoryAccess::Inaccessible).isFalse() ||
      !memaccess.canRead(MemoryAccess::Errno).isFalse() ||
      !memaccess.canRead(MemoryAccess::Other).isFalse())
    call_ranges = memaccess.canOnlyRead(MemoryAccess::Inaccessible).isTrue()
                    ? analysis.ranges_fn_calls.project(name)
                    : analysis.ranges_fn_calls;

  call_ranges.keep_only_writes();

  if (ret_arg_ty && (*ret_arg_ty == out_type).isFalse()) {
    ret_arg = out_type.fromInt(ret_arg_ty->toInt(*this, std::move(ret_arg)));
  }

  // source may create new fn symbols, target just references src symbols
  if (isSource()) {
    auto &calls_fn = fn_call_data[name];
    auto call_data_pair
      = calls_fn.try_emplace(
          { std::move(inputs), std::move(ptr_inputs), std::move(call_ranges),
            memaccess.canReadSomething().isFalse()
              ? memory.dupNoRead() : memory.dup(),
            memaccess, noret, willret });
    auto &I = call_data_pair.first;
    bool inserted = call_data_pair.second;

    if (inserted) {
      StateValue output;
      vector<Memory::FnRetData> ret_data;
      string npname = name + "#np";

      auto mk_np = [&](expr &&np) {
        return noundef ? std::move(np) : expr::mkFreshVar(npname.c_str(), np);
      };

      function<StateValue(const Type &)> mk_output
        = [&](const Type &ty) -> StateValue {
        if (ty.isPtrType()) {
          auto [val, mem]
            = memory.mkFnRet(name.c_str(), I->first.args_ptr, noalias);
          ret_data.emplace_back(std::move(mem));
          return { std::move(val), mk_np(true) };
        }

        if (!hasPtr(ty)) {
          auto dummy = ty.getDummyValue(true);
          return { expr::mkFreshVar(name.c_str(), dummy.value),
                   mk_np(std::move(dummy.non_poison)) };
        }

        assert(ty.isAggregateType());
        auto agg = ty.getAsAggregateType();
        vector<StateValue> vals;
        for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
          if (!agg->isPadding(i))
            vals.emplace_back(mk_output(agg->getChild(i)));
        }
        return agg->aggregateVals(vals);
      };

      output = ret_arg_ty ? std::move(ret_arg) : mk_output(out_type);
      if (ret_arg_ty && ret_arg_ty->isPtrType())
        ret_data.emplace_back(Memory::FnRetData());

      // Indirect calls may be changed into direct in tgt
      // Account for this if we have declarations with a returned argument
      // to limit the behavior of the SMT var.
      if (is_indirect) {
        for (auto &decl : getFn().getFnDecls()) {
          if (decl.inputs.size() != ret_args.size() || decl.is_varargs)
            continue;
          unsigned idx = 0;
          for (auto &[ty, attrs] : decl.inputs) {
            if (attrs.has(ParamAttrs::Returned)) {
              auto &ret = ret_args[idx];
              if (ret.value.isSameTypeOf(output.value)) {
                auto cmp = isgvar(decl);
                if (cmp.isValid())
                  output = StateValue::mkIf(cmp, ret, output);
              }
              break;
            }
            ++idx;
          }
        }
      }

      I->second
        = { std::move(output), expr::mkFreshVar((name + "#ub").c_str(), false),
            (noret || willret)
              ? expr(noret)
              : expr::mkFreshVar((name + "#noreturn").c_str(), false),
            memory.mkCallState(name, attrs.has(FnAttrs::NoFree),
                               I->first.args_ptr.size(), memaccess),
            std::move(ret_data) };

      // add equality constraints between source's function calls
      for (auto II = calls_fn.begin(), E = calls_fn.end(); II != E; ++II) {
        if (II == I)
          continue;
        auto in_eq = I->first.implies(II->first);
        if (!in_eq.isFalse())
          fn_call_pre &= in_eq.implies(I->second.implies(II->second, out_type));
      }
    }

    addUB(I->second.ub);
    addNoReturn(I->second.noreturns);
    retval = I->second.retval;
    memory.setState(I->second.callstate, memaccess, I->first.args_ptr,
                    inaccessible_bid);
  }
  else {
    // target: this fn call must match one from the source, otherwise it's UB
    ChoiceExpr<FnCallOutput> data;

    for (auto &[in, out] : fn_call_data[name]) {
      auto refined = in.refinedBy(*this, name, inaccessible_bid, inputs,
                                  ptr_inputs, call_ranges, memory, memaccess,
                                  noret, willret);
      data.add(ret_arg_ty ? out.replace(ret_arg) : out, std::move(refined));
    }

    if (data) {
      auto [d, domain, qvar, pre] = std::move(data)();
      addUB(std::move(domain));
      addUB(std::move(d.ub));
      addNoReturn(std::move(d.noreturns));

      // functions never return poison in assembly
      if (isAsmMode())
        d.retval.setNotPoison();

      if (noalias) {
        // no alias functions in tgt must allocate a local block on each call
        // bid may be different from that of src
        unsigned i = 0;
        auto &ret_data = d.ret_data;
        function<StateValue(const Type &, StateValue &&)> mk_output
          = [&](const Type &ty, StateValue &&val) -> StateValue {
          if (ty.isPtrType()) {
            return { memory.mkFnRet(name.c_str(), ptr_inputs, noalias,
                                    &ret_data[i++]).first,
                     std::move(val.non_poison) };
          }

          if (!hasPtr(ty))
            return std::move(val);

          assert(ty.isAggregateType());
          auto agg = ty.getAsAggregateType();
          vector<StateValue> vals;
          for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
            vals.emplace_back(
              mk_output(agg->getChild(i), agg->extract(val, i)));
          }
          return agg->aggregateVals(vals);
        };
        retval = mk_output(out_type, std::move(d.retval));
      } else
        retval = std::move(d.retval);

      memory.setState(d.callstate, memaccess, ptr_inputs, inaccessible_bid);

      fn_call_pre &= pre;
      if (qvar.isValid())
        fn_call_qvars.emplace(std::move(qvar));
    } else {
      addUB(expr(false));
      retval = out_type.getDummyValue(false);
    }
  }

  analysis.ranges_fn_calls.inc(name, memaccess);

  return retval;
}

void State::doesApproximation(string &&name, optional<expr> e) {
  used_approximations.emplace(std::move(name), std::move(e));
}

void State::addQuantVar(const expr &var) {
  quantified_vars.emplace(var);
}

void State::addNonDetVar(const expr &var) {
  nondet_vars.emplace(var);
}

expr State::getFreshNondetVar(const char *prefix, const expr &type) {
  expr var = expr::mkFreshVar(prefix, type);
  addNonDetVar(var);
  return var;
}

void State::addFnQuantVar(const expr &var) {
  fn_call_qvars.emplace(var);
}

void State::addUndefVar(expr &&var) {
  undef_vars.emplace(std::move(var));
}

void State::resetUndefVars(bool quantify) {
  ((isSource() || !quantify) ? quantified_vars : fn_call_qvars)
    .insert(undef_vars.begin(), undef_vars.end());
  undef_vars.clear();
}

StateValue State::rewriteUndef(StateValue &&val, const set<expr> &undef_vars) {
  if (undef_vars.empty())
    return std::move(val);
  if (hit_half_memory_limit())
    throw_oom_exception();

  vector<pair<expr, expr>> repls;
  for (auto &var : undef_vars) {
    auto newvar = expr::mkFreshVar("undef", var);
    repls.emplace_back(var, newvar);
    addUndefVar(std::move(newvar));
  }
  return val.subst(repls);
}

expr State::rewriteUndef(expr &&val, const set<expr> &undef_vars) {
  return rewriteUndef({std::move(val), expr()}, undef_vars).value;
}

void State::finishInitializer() {
  is_initialization_phase = false;

  const Memory *mem = &memory;
  // if we have an init block, the unconditional jump std::moved the memory
  if (!predecessor_data.empty()) {
    assert(predecessor_data.size() == 1);
    mem = &predecessor_data.begin()->second.begin()->second.mem.begin()->first;
  }
  return_memory = DisjointExpr(mem->dup());

  if (auto *ret = getFn().getReturnedInput()) {
    returned_input = (*this)[*ret];
    resetUndefVars(true);
  }
}

bool State::isImplied(const expr &e, const expr &e_domain) {
  if (domain.UB.contains(e))
    return true;

  if (check_expr((e_domain && domain()).notImplies(e), "UB inference", true)
        .isUnsat()) {
    domain.UB.add(e_domain.implies(e));
    return true;
  }
  return false;
}

expr State::sinkDomain(bool include_ub) const {
  auto I = predecessor_data.find(&f.getSinkBB());
  if (I == predecessor_data.end())
    return false;

  OrExpr ret;
  for (auto &[src, data] : I->second) {
    ret.add(data.path() && (include_ub ? data.UB.factor()() : true));
  }
  return ret();
}

const StateValue& State::returnValCached() {
  if (auto *v = get_if<DisjointExpr<StateValue>>(&return_val)) {
    return_val = *std::move(*v)();
    auto &val = get<StateValue>(return_val);
    if (isAsmMode() && !val.non_poison.isTrue()) {
      // there is no poison in asm mode
      val.value = expr::mkIf(
        val.non_poison, val.value, expr::mkFreshVar("nondet", val.value));
      val.non_poison = true;
    }
  }
  return get<StateValue>(return_val);
}

Memory& State::returnMemory() {
  if (auto *m = get_if<DisjointExpr<Memory>>(&return_memory)) {
    return_memory = *std::move(*m)();
  }
  return get<Memory>(return_memory);
}

expr State::getJumpCond(const BasicBlock &src, const BasicBlock &dst) const {
  auto I = predecessor_data.find(&dst);
  if (I == predecessor_data.end())
    return false;

  auto J = I->second.find(&src);
  return J == I->second.end() ? expr(false)
                              : J->second.path() && J->second.UB.factor()();
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

bool State::isGVUsed(unsigned bid) const {
  for (auto &[gv_name, data] : glbvar_bids) {
    if (bid == data.first)
      return getFn().getUsers().count(getFn().getGlobalVar(gv_name));
  }
  assert(false);
  return false;
}

void State::syncSEdataWithSrc(State &src) {
  assert(glbvar_bids.empty());
  assert(src.isSource() && !isSource());
  glbvar_bids = src.glbvar_bids;
  for (auto &[gv_name, data] : glbvar_bids) {
    data.second = false;
  }
  fn_call_data = std::move(src.fn_call_data);
  inaccessiblemem_bids = std::move(src.inaccessiblemem_bids);
  memory.syncWithSrc(src.returnMemory());

  src_state = &src;
  for (auto &[bb, srcs] : src.predecessor_data) {
    OrExpr path;
    for (auto &[src, data] : srcs) {
      path.add(data.path);
    }
    src_bb_paths[std::move(path)()].emplace_back(bb);
  }
}

void State::mkAxioms(State &tgt) {
  assert(isSource() && !tgt.isSource());
  returnMemory().mkAxioms(tgt.returnMemory());

  if (has_indirect_fncalls) {
    for (auto &decl : f.getFnDecls()) {
      if (auto gv = f.getGlobalVar(decl.name)) {
        Pointer ptr(memory, (*this)[*gv].value);
        addAxiom(!ptr.isLocal());
        addAxiom(ptr.getOffset() == 0);
        addAxiom(
          expr::mkUF("#fndeclty", { ptr.getShortBid() }, expr::mkUInt(0, 32))
            == decl.hash());
      }
    }
  }
}

void State::cleanup() {
  src_bb_paths.clear();
  undef_vars.clear();
  fn_call_data.clear();
  domain = {};
  analysis = {};
  var_args_data = {};
}

}
