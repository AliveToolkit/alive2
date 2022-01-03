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

template<class T>
static T intersect_set(const T &a, const T &b) {
  T results;
  set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                   inserter(results, results.begin()));
  return results;
}

void State::ValueAnalysis::meet_with(const State::ValueAnalysis &other) {
  non_poison_vals = intersect_set(non_poison_vals, other.non_poison_vals);
  non_undef_vals = intersect_set(non_undef_vals, other.non_undef_vals);
  unused_vars = intersect_set(unused_vars, other.unused_vars);

  for (auto &[fn, pair] : other.ranges_fn_calls) {
    auto &[calls, inaccessiblememonly] = pair;
    auto [I, inserted] = ranges_fn_calls.try_emplace(fn, pair);
    if (inserted) {
      I->second.first.emplace(0);
    } else {
      assert(I->second.second == inaccessiblememonly);
      I->second.first.insert(calls.begin(), calls.end());
    }
  }

  for (auto &[fn, pair] : ranges_fn_calls) {
    auto &[calls, inaccessiblememonly] = pair;
    if (!other.ranges_fn_calls.count(fn))
      calls.emplace(0);
  }
}

void State::ValueAnalysis::FnCallRanges::inc(const std::string &name,
                                             bool inaccessible_or_args_memonly) {
  auto [I, inserted] = try_emplace(name);
  if (inserted) {
    I->second.first.emplace(1);
    I->second.second = inaccessible_or_args_memonly;
  } else {
    set<unsigned> new_set;
    for (unsigned n : I->second.first) {
      new_set.emplace(n+1);
    }
    I->second.first   = move(new_set);
    I->second.second |= inaccessible_or_args_memonly;
  }
}

bool
State::ValueAnalysis::FnCallRanges::overlaps(const FnCallRanges &other) const {
  for (auto &[fn, pair] : *this) {
    auto &[calls, inaccessiblememonly] = pair;
    if (inaccessiblememonly)
      continue;

    auto I = other.find(fn);
    if (I == other.end()) {
      if (calls.count(0))
        continue;
      return false;
    }
    if (I->second.second) // argmemonly
      continue;
    if (intersect_set(calls, I->second.first).empty())
      return false;
  }

  for (auto &[fn, pair] : other) {
    auto &[calls, inaccessiblememonly] = pair;
    if (inaccessiblememonly)
      continue;

    if (!calls.count(0) && !count(fn))
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
  ranges.emplace(name, make_pair(I->second.first, false));
  return ranges;
}

State::VarArgsData
State::VarArgsData::mkIf(const expr &cond, const VarArgsData &then,
                         const VarArgsData &els) {
  VarArgsData ret;
  for (auto &[ptr, entry] : then.data) {
    auto other = els.data.find(ptr);
    if (other == els.data.end()) {
      ret.data.try_emplace(ptr, cond && entry.alive, expr(entry.next_arg),
                           expr(entry.num_args), expr(entry.is_va_start),
                           expr(entry.active));
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
    ret.data.try_emplace(ptr, !cond && entry.alive, expr(entry.next_arg),
                         expr(entry.num_args), expr(entry.is_va_start),
                         expr(entry.active));
  }

  return ret;
}

State::State(const Function &f, bool source)
  : f(f), source(source), memory(*this),
    return_val(f.getType().getDummyValue(false)), return_memory(memory) {}

void State::resetGlobals() {
  Memory::resetGlobals();
}

const State::ValTy& State::exec(const Value &v) {
  assert(undef_vars.empty());
  auto val = v.toSMT(*this);
  ENSURE(values_map.try_emplace(&v, (unsigned)values.size()).second);
  values.emplace_back(&v, ValTy{move(val), domain.UB(), move(undef_vars)});
  analysis.unused_vars.insert(&v);

  // cleanup potentially used temporary values due to undef rewriting
  while (i_tmp_values > 0) {
    tmp_values[--i_tmp_values] = StateValue();
  }

  return get<1>(values.back());
}

static expr eq_except_padding(const Type &ty, const expr &e1, const expr &e2) {
  const auto *aty = ty.getAsAggregateType();
  if (!aty)
    return e1 == e2;

  StateValue sv1{expr(e1), expr()};
  StateValue sv2{expr(e2), expr()};
  expr result = true;

  for (unsigned i = 0; i < aty->numElementsConst(); ++i) {
    if (aty->isPadding(i))
      continue;

    result &= eq_except_padding(aty->getChild(i), aty->extract(sv1, i).value,
                                aty->extract(sv2, i).value);
  }
  return result;
}

expr State::strip_undef_and_add_ub(const Value &val, const expr &e) {
  if (isUndef(e)) {
    addUB(expr(false));
    return expr::mkUInt(0, e);
  }

  auto is_undef_cond = [](const expr &e, const expr &var) {
    expr lhs, rhs;
    // (= #b0 isundef_%var)
    if (e.isEq(lhs, rhs)) {
      return (lhs.isZero() && Input::isUndefMask(rhs, var)) ||
             (rhs.isZero() && Input::isUndefMask(lhs, var));
    }
    return false;
  };

  // pointer undef vars show up like (concat 0 undef)
  auto is_undef_or_concat = [&](const expr &e) {
    if (isUndef(e))
      return true;

    expr a, b;
    return e.isConcat(a, b) && a.isZero() && isUndef(b);
  };

  auto is_if_undef = [&](const expr &e, expr &var, expr &not_undef) {
    expr undef;
    // (ite (= #b0 isundef_%var) %var undef)
    return e.isIf(not_undef, var, undef) &&
           is_undef_or_concat(undef) &&
           is_undef_cond(not_undef, var);
  };

  // e2: stripped expression
  auto is_if_undef_or_add = [&](const expr &e, expr &var, expr &not_undef,
                                expr &e2) {
    // when e = (ite (= #b0 isundef_%var) %var undef):
    //   var = %var, e2 = %var
    // when e = (bvadd const (ite (= #b0 isundef_%var) %var undef))
    //   var = %var, e2 = const + %var
    if (is_if_undef(e, var, not_undef)) {
      e2 = var;
      return true;
    }

    expr a, b;
    if (e.isAdd(a, b)) {
      if (b.isConst() && is_if_undef(a, var, not_undef)) {
        e2 = b + var;
        return true;
      } else if (a.isConst() && is_if_undef(b, var, not_undef)) {
        e2 = a + var;
        return true;
      }
    }
    return false;
  };

  expr c, a, b, lhs, rhs;

  // two variants
  // 1) boolean
  if (is_if_undef(e, a, b)) {
    addUB(move(b));
    return a;
  }

  auto has_undef = [&](const expr &e) {
    auto vars = e.vars();
    return any_of(vars.begin(), vars.end(),
                  [&](auto &v) { return isUndef(v); });
  };

  auto mark_notundef = [&](const expr &var) {
    auto name = var.fn_name();
    for (auto &v : values_map) {
      if (v.first->getName() == name) {
        analysis.non_undef_vals.emplace(v.first, var);
        return;
      }
    }
  };

  if (e.isIf(c, a, b) && a.isConst() && b.isConst()) {
    expr val, val2, newe, newe2, not_undef, not_undef2;
    // (ite (= val (ite (= #b0 isundef_%var) %var undef)) #b1 #b0)
    // (ite (= val (bvadd c (ite (= #b0 isundef_%var) %var undef)) #b1 #b0)
    if (c.isEq(lhs, rhs)) {
      if (is_if_undef_or_add(lhs, val, not_undef, newe) && !has_undef(rhs)) {
        addUB(move(not_undef));
        mark_notundef(val);
        // %var == rhs
        // (bvadd c %var) == rhs
        return expr::mkIf(newe == rhs, a, b);
      }
      if (is_if_undef_or_add(rhs, val, not_undef, newe) && !has_undef(lhs)) {
        addUB(move(not_undef));
        mark_notundef(val);
        return expr::mkIf(lhs == newe, a, b);
      }
      if (is_if_undef_or_add(lhs, val, not_undef, newe) &&
          is_if_undef_or_add(rhs, val2, not_undef2, newe2)) {
        addUB(move(not_undef));
        addUB(move(not_undef2));
        mark_notundef(val);
        mark_notundef(val2);
        return expr::mkIf(newe == newe2, a, b);
      }
    }

    if (c.isSLE(lhs, rhs)) {
      // (ite (bvsle val (ite (= #b0 isundef_%var) %var undef)) #b1 #b0)
      // (ite (bvsle val (bvadd c (ite (= #b0 isundef_%var) %var undef))
      //       #b1 #b0)
      if (is_if_undef_or_add(rhs, val, not_undef, newe) && !has_undef(lhs)) {
        expr cond = lhs == expr::IntSMin(lhs.bits());
        addUB(not_undef || cond);
        if (cond.isFalse())
          mark_notundef(val);
        // lhs <=s %var
        // lhs <=s (bvadd c %var)
        return expr::mkIf(lhs.sle(newe), a, b);
      }

      // (ite (bvsle (ite (= #b0 isundef_%var) %var undef) val) #b1 #b0)
      // (ite (bvsle (bvadd c (ite (= #b0 isundef_%var) %var undef)) val)
      //       #b1 #b0)
      if (is_if_undef_or_add(lhs, val, not_undef, newe) && !has_undef(rhs)) {
        expr cond = rhs == expr::IntSMax(rhs.bits());
        addUB(not_undef || cond);
        if (cond.isFalse())
          mark_notundef(val);
        return expr::mkIf(newe.sle(rhs), a, b);
      }

      // undef <= undef
      if (is_if_undef_or_add(lhs, val, not_undef, newe) &&
          is_if_undef_or_add(rhs, val2, not_undef2, newe2)) {
        addUB((not_undef && not_undef2) ||
              (not_undef && newe == expr::IntSMin(lhs.bits())) ||
              (not_undef2 && newe2 == expr::IntSMax(rhs.bits())));
        return expr::mkIf(newe.sle(newe2), a, b);
      }
    }

    if (c.isULE(lhs, rhs)) {
      // (ite (bvule val (ite (= #b0 isundef_%var) %var undef)) #b1 #b0)
      // (ite (bvule val (bvadd c (ite (= #b0 isundef_%var) %var undef)))
      //       #b1 #b0)
      if (is_if_undef_or_add(rhs, val, not_undef, newe) && !has_undef(lhs)) {
        expr cond = lhs == 0;
        addUB(not_undef || cond);
        if (cond.isFalse())
          mark_notundef(val);
        // lhs <=u %var
        // lhs <=u (bvadd c %var)
        return expr::mkIf(lhs.ule(newe), a, b);
      }

      // (ite (bvule (ite (= #b0 isundef_%var) %var undef) val) #b1 #b0)
      // (ite (bvule (bvadd c (ite (= #b0 isundef_%var) %var undef)) %val)
      //       #b1 #b0)
      if (is_if_undef_or_add(lhs, val, not_undef, newe) && !has_undef(rhs)) {
        expr cond = rhs == expr::mkInt(-1, rhs);
        addUB(not_undef || cond);
        if (cond.isFalse())
          mark_notundef(val);
        return expr::mkIf(newe.ule(rhs), a, b);
      }

      // undef <= undef
      if (is_if_undef_or_add(lhs, val, not_undef, newe) &&
          is_if_undef_or_add(rhs, val2, not_undef2, newe2)) {
        addUB((not_undef && not_undef2) ||
              (not_undef && newe == 0) ||
              (not_undef2 && newe2 == expr::mkInt(-1, rhs)));
        return expr::mkIf(newe.ule(newe2), a, b);
      }
    }
  }

  // 2) (or (and |isundef_%var| undef) (and %var (not |isundef_%var|)))
  // TODO

  // check if original expression is equal to an expression where undefs are
  // fixed to a const value
  vector<pair<expr,expr>> repls;
  for (auto &undef : undef_vars) {
    expr newv = expr::mkFreshVar("#undef'", undef);
    addQuantVar(newv);
    repls.emplace_back(undef, move(newv));
  }
  addUB(eq_except_padding(val.getType(), e, e.subst(repls)));
  return e;
}

StateValue* State::no_more_tmp_slots() {
  if (i_tmp_values < tmp_values.size())
    return nullptr;
  throw AliveException("Too many temporaries", false);
}

const StateValue& State::operator[](const Value &val) {
  auto &[var, val_uvars] = values[values_map.at(&val)];
  auto &[sval, _ub, uvars] = val_uvars;

  auto undef_itr = analysis.non_undef_vals.find(&val);
  bool is_non_undef = undef_itr != analysis.non_undef_vals.end();
  bool is_non_poison = analysis.non_poison_vals.count(&val);

  auto simplify = [&](StateValue &sv0, bool use_new_slot) -> StateValue& {
    if (!is_non_undef && !is_non_poison)
      return sv0;

    if (use_new_slot) {
      if (auto ret = no_more_tmp_slots())
        return *ret;
      assert(i_tmp_values < tmp_values.size());
      tmp_values[i_tmp_values++] = sv0;
    }
    assert(i_tmp_values > 0);
    StateValue &sv_new = tmp_values[i_tmp_values - 1];
    if (is_non_undef) {
      sv_new.value = undef_itr->second;
    }
    if (is_non_poison) {
      const expr &np = sv_new.non_poison;
      sv_new.non_poison = np.isBool() ? true : expr::mkUInt(0, np);
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
    throw AliveException("Out of memory; skipping function.", false);

  auto sval_new = sval.subst(repls);
  if (sval_new.eq(sval)) {
    uvars.clear();
    return sval;
  }

  for (auto &p : repls) {
    undef_vars.emplace(move(p.second));
  }

  if (auto ret = no_more_tmp_slots())
    return *ret;

  assert(i_tmp_values < tmp_values.size());
  tmp_values[i_tmp_values++] = move(sval_new);
  return simplify(tmp_values[i_tmp_values - 1], false);
}

const StateValue& State::getAndAddUndefs(const Value &val) {
  auto &v = (*this)[val];
  for (auto uvar: at(val).undef_vars)
    addQuantVar(move(uvar));
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
State::getAndAddPoisonUB(const Value &val, bool undef_ub_too) {
  auto &sv = (*this)[val];

  bool poison_already_added = !analysis.non_poison_vals.insert(&val).second;
  if (poison_already_added && !undef_ub_too)
    return sv;

  expr v = sv.value;

  if (undef_ub_too) {
    auto I = analysis.non_undef_vals.find(&val);
    if (I != analysis.non_undef_vals.end()) {
      v = I->second;
    } else {
      v = strip_undef_and_add_ub(val, v);
      analysis.non_undef_vals.emplace(&val, v);
    }
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

  if (auto ret = no_more_tmp_slots())
    return *ret;

  assert(i_tmp_values < tmp_values.size());
  return tmp_values[i_tmp_values++] = { move(v),
           sv.non_poison.isBool() ? true : expr::mkInt(-1, sv.non_poison) };
}

const State::ValTy& State::at(const Value &val) const {
  return get<1>(values[values_map.at(&val)]);
}

const OrExpr* State::jumpCondFrom(const BasicBlock &bb) const {
  auto &pres = predecessor_data.at(current_bb);
  auto I = pres.find(&bb);
  return I == pres.end() ? nullptr : &I->second.path;
}

bool State::isUndef(const expr &e) const {
  return undef_vars.count(e) != 0;
}

bool State::startBB(const BasicBlock &bb) {
  assert(undef_vars.empty());
  ENSURE(seen_bbs.emplace(&bb).second);
  current_bb = &bb;

  if (&f.getFirstBB() == &bb)
    return true;

  auto I = predecessor_data.find(&bb);
  if (I == predecessor_data.end())
    return false;

  DisjointExpr<Memory> in_memory;
  DisjointExpr<expr> UB;
  DisjointExpr<VarArgsData> var_args_in;
  OrExpr path;

  bool isFirst = true;
  for (auto &[src, data] : I->second) {
    path.add(data.path);
    expr p = data.path();
    UB.add_disj(data.UB, p);
    in_memory.add_disj(data.mem, p);
    var_args_in.add(data.var_args, move(p));
    domain.undef_vars.insert(data.undef_vars.begin(), data.undef_vars.end());

    if (isFirst)
      analysis = data.analysis;
    else
      analysis.meet_with(data.analysis);
    isFirst = false;
  }

  domain.path = path();
  domain.UB = *UB();
  memory = *in_memory();
  var_args_data = *var_args_in();

  return domain;
}

void State::addJump(const BasicBlock &dst0, expr &&cond) {
  if (cond.isFalse())
    return;

  auto dst = &dst0;
  if (seen_bbs.count(dst)) {
    dst = &f.getSinkBB();
  }

  cond &= domain.path;
  auto &data = predecessor_data[dst][current_bb];
  data.mem.add(memory, cond);
  data.UB.add(domain.UB(), cond);
  data.path.add(move(cond));
  data.undef_vars.insert(undef_vars.begin(), undef_vars.end());
  data.undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  data.analysis = analysis;
  data.var_args = var_args_data;
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
  expr cond_false = cond == 0;
  addJump(dst_true,  !cond_false);
  addJump(dst_false, move(cond_false));
  addUB(expr(false));
}

void State::addReturn(StateValue &&val) {
  return_val.add(move(val), domain.path);
  return_memory.add(memory, domain.path);
  auto dom = domain();
  return_domain.add(expr(dom));
  function_domain.add(move(dom));
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

void State::addNoReturn(const expr &cond) {
  if (cond.isFalse())
    return;
  return_memory.add(memory, domain.path && cond);
  function_domain.add(domain() && cond);
  return_undef_vars.insert(undef_vars.begin(), undef_vars.end());
  return_undef_vars.insert(domain.undef_vars.begin(), domain.undef_vars.end());
  if (cond.isTrue())
    undef_vars.clear();
  addUB(!cond);
}

expr State::FnCallInput::operator==(const FnCallInput &rhs) const {
  if (readsmem != rhs.readsmem ||
      argmemonly != rhs.argmemonly ||
      inaccessiblememonly != rhs.inaccessiblememonly ||
      noret != rhs.noret || willret != rhs.willret ||
      (readsmem && (fncall_ranges != rhs.fncall_ranges || is_neq(m <=> rhs.m))))
    return false;

  AndExpr eq;
  for (unsigned i = 0, e = args_nonptr.size(); i != e; ++i) {
    eq.add(args_nonptr[i] == rhs.args_nonptr[i]);
  }

  for (unsigned i = 0, e = args_ptr.size(); i != e; ++i) {
    eq.add(args_ptr[i] == rhs.args_ptr[i]);
  }
  return eq();
}

expr State::FnCallInput::refinedBy(
  State &s, unsigned modifies_bid, const vector<StateValue> &args_nonptr2,
  const vector<Memory::PtrInput> &args_ptr2,
  const ValueAnalysis::FnCallRanges &fncall_ranges2,
  const Memory &m2, bool readsmem2, bool argmemonly2, bool inaccessiblememonly2,
  bool noret2, bool willret2) const {

  if (readsmem != readsmem2 || argmemonly != argmemonly2 ||
      inaccessiblememonly != inaccessiblememonly2 ||
      noret != noret2 || willret != willret2 ||
      (readsmem && !fncall_ranges.overlaps(fncall_ranges2)))
    return false;

  AndExpr refines;
  assert(args_nonptr.size() == args_nonptr2.size());
  for (unsigned i = 0, e = args_nonptr.size(); i != e; ++i) {
    refines.add(args_nonptr[i].non_poison.implies(
      args_nonptr[i].value == args_nonptr2[i].value &&
      args_nonptr2[i].non_poison));
  }

  if (!refines)
    return false;

  set<expr> undef_vars;
  if (!inaccessiblememonly) {
    assert(args_ptr.size() == args_ptr2.size());
    for (unsigned i = 0, e = args_ptr.size(); i != e; ++i) {
      // TODO: needs to take read/read2 as input to control if mem blocks
      // need to be compared
      auto &[ptr_in, byval, is_nocapture] = args_ptr[i];
      auto &[ptr_in2, byval2, is_nocapture2] = args_ptr2[i];
      if (byval != byval2 || is_nocapture != is_nocapture2)
        return false;

      expr eq_val = Pointer(m, ptr_in.value)
                      .fninputRefined(Pointer(m2, ptr_in2.value),
                                      undef_vars, byval2);
      refines.add(ptr_in.non_poison.implies(eq_val && ptr_in2.non_poison));

      if (!refines)
        return false;
    }
  }

  for (auto &v : undef_vars)
    s.addFnQuantVar(v);

  if (readsmem) {
    vector<Memory::PtrInput> dummy1, dummy2;
    auto restrict_ptrs = argmemonly ? &args_ptr : nullptr;
    auto restrict_ptrs2 = argmemonly ? &args_ptr2 : nullptr;
    if (modifies_bid != -1u) {
      dummy1.emplace_back(
        StateValue(Pointer(m, modifies_bid, false).release(), true), 0, false);
      dummy2.emplace_back(
        StateValue(Pointer(m2, modifies_bid, false).release(), true), 0, false);
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

State::FnCallOutput State::FnCallOutput::mkIf(const expr &cond,
                                              const FnCallOutput &a,
                                              const FnCallOutput &b) {
  FnCallOutput ret;
  ret.ub = expr::mkIf(cond, a.ub, b.ub);
  ret.noreturns = expr::mkIf(cond, a.noreturns, b.noreturns);
  ret.callstate = Memory::CallState::mkIf(cond, a.callstate, b.callstate);
  assert(a.retvals.size() == b.retvals.size());
  for (unsigned i = 0, e = a.retvals.size(); i != e; ++i) {
    ret.retvals.emplace_back(
      StateValue::mkIf(cond, a.retvals[i], b.retvals[i]));
    ret.ret_data.emplace_back(
      Memory::FnRetData::mkIf(cond, a.ret_data[i], b.ret_data[i]));
  }
  return ret;
}

expr State::FnCallOutput::operator==(const FnCallOutput &rhs) const {
  expr ret = ub == rhs.ub;
  ret &= noreturns == rhs.noreturns;
  for (unsigned i = 0, e = retvals.size(); i != e; ++i) {
    ret &= retvals[i] == rhs.retvals[i];
  }
  ret &= callstate == rhs.callstate;
  return ret;
}

vector<StateValue>
State::addFnCall(const string &name, vector<StateValue> &&inputs,
                 vector<Memory::PtrInput> &&ptr_inputs,
                 const vector<Type*> &out_types, const FnAttrs &attrs) {
  bool reads_memory = !attrs.has(FnAttrs::NoRead);
  bool writes_memory = !attrs.has(FnAttrs::NoWrite);
  bool argmemonly = attrs.has(FnAttrs::ArgMemOnly);
  bool inaccessiblememonly = attrs.has(FnAttrs::InaccessibleMemOnly);
  bool noret = attrs.has(FnAttrs::NoReturn);
  bool willret = attrs.has(FnAttrs::WillReturn);
  bool noundef = attrs.has(FnAttrs::NoUndef);
  bool noalias = attrs.has(FnAttrs::NoAlias);

  assert(!noret || !willret);

  bool all_valid = std::all_of(inputs.begin(), inputs.end(),
                                [](auto &v) { return v.isValid(); }) &&
                   std::all_of(ptr_inputs.begin(), ptr_inputs.end(),
                                [](auto &v) { return v.val.isValid(); });

  if (!all_valid) {
    addUB(expr());
    return vector<StateValue>(out_types.size());
  }

  if (writes_memory) {
    for (auto &v : ptr_inputs) {
      if (!v.byval && !v.nocapture && !v.val.non_poison.isFalse())
        memory.escapeLocalPtr(v.val.value);
    }
  }

  vector<StateValue> retval;
  unsigned modifies_bid = -1u;
  if (inaccessiblememonly)
    modifies_bid
      = inaccessiblemem_bids.try_emplace(name, inaccessiblemem_bids.size())
                            .first->second;

  State::ValueAnalysis::FnCallRanges call_ranges;
  if (reads_memory && !argmemonly)
    call_ranges = inaccessiblememonly
                    ? analysis.ranges_fn_calls.project(name)
                    : analysis.ranges_fn_calls;

  // source may create new fn symbols, target just references src symbols
  if (isSource()) {
    auto &calls_fn = fn_call_data[name];
    auto call_data_pair
      = calls_fn.try_emplace(
          { move(inputs), move(ptr_inputs), move(call_ranges),
            reads_memory ? memory : Memory(*this),
            reads_memory, argmemonly, inaccessiblememonly, noret, willret });
    auto &I = call_data_pair.first;
    bool inserted = call_data_pair.second;

    if (inserted) {
      auto mk_val = [&](const Type &t, const string &name) {
        if (t.isPtrType())
          return memory.mkFnRet(name.c_str(), I->first.args_ptr, noalias);

         return make_pair(
           expr::mkFreshVar(name.c_str(), t.getDummyValue(false).value),
           Memory::FnRetData());
      };

      vector<StateValue> values;
      vector<Memory::FnRetData> ret_data;
      string valname = name + "#val";
      string npname = name + "#np";
      for (auto t : out_types) {
        auto [val, data] = mk_val(*t, valname);
        values.emplace_back(
          move(val),
          noundef ? expr(true) : expr::mkFreshVar(npname.c_str(), false));
        ret_data.emplace_back(move(data));
      }

      I->second
        = { move(values), expr::mkFreshVar((name + "#ub").c_str(), false),
            (noret || willret)
              ? expr(noret)
              : expr::mkFreshVar((name + "#noreturn").c_str(), false),
            writes_memory
              ? memory.mkCallState(name,
                                   attrs.has(FnAttrs::NoFree),
                                   inaccessiblememonly)
              : Memory::CallState(), move(ret_data) };

      // add equality constraints between source's function calls
      for (auto II = calls_fn.begin(), E = calls_fn.end(); II != E; ++II) {
        if (II == I)
          continue;
        auto in_eq = I->first == II->first;
        if (!in_eq.isFalse())
          fn_call_pre &= in_eq.implies(I->second == II->second);
      }
    }

    addUB(I->second.ub);
    addNoReturn(I->second.noreturns);
    retval = I->second.retvals;
    if (writes_memory)
      memory.setState(I->second.callstate,
                      argmemonly ? &I->first.args_ptr : nullptr,
                      modifies_bid);
  }
  else {
    // target: this fn call must match one from the source, otherwise it's UB
    ChoiceExpr<FnCallOutput> data;

    for (auto &[in, out] : fn_call_data[name]) {
      auto refined = in.refinedBy(*this, modifies_bid, inputs, ptr_inputs,
                                  call_ranges, memory, reads_memory, argmemonly,
                                  inaccessiblememonly, noret, willret);
      data.add(out, move(refined));
    }

    if (data) {
      auto [d, domain, qvar, pre] = data();
      addUB(move(domain));
      addUB(move(d.ub));
      addNoReturn(move(d.noreturns));

      if (noalias) {
        // no alias functions in tgt must allocate a local block on each call
        // bid may be different from that of src
        unsigned i = 0;
        for (auto t : out_types) {
          retval.emplace_back(
            t->isPtrType()
              ? StateValue(memory.mkFnRet(name.c_str(), ptr_inputs, noalias,
                                          &d.ret_data[i]).first,
                           move(d.retvals[i].non_poison))
              : move(d.retvals[i]));
          ++i;
        }
      } else
        retval = move(d.retvals);

      if (writes_memory)
        memory.setState(d.callstate,
                        argmemonly ? &ptr_inputs : nullptr,
                        modifies_bid);

      fn_call_pre &= pre;
      if (qvar.isValid())
        fn_call_qvars.emplace(move(qvar));
    } else {
      addUB(expr(false));
      for (auto *t : out_types) {
        retval.emplace_back(t->getDummyValue(false));
      }
    }
  }

  if (writes_memory)
    analysis.ranges_fn_calls.inc(name, argmemonly | inaccessiblememonly);

  return retval;
}

void State::doesApproximation(string &&name, optional<expr> e) {
  used_approximations.emplace(move(name), move(e));
}

void State::addQuantVar(const expr &var) {
  quantified_vars.emplace(var);
}

void State::addFnQuantVar(const expr &var) {
  fn_call_qvars.emplace(var);
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

expr State::rewriteUndef(expr &&val, const set<expr> &undef_vars) {
  return rewriteUndef({move(val), expr()}, undef_vars).value;
}

void State::finishInitializer() {
  is_initialization_phase = false;
}

expr State::sinkDomain() const {
  auto I = predecessor_data.find(&f.getSinkBB());
  if (I == predecessor_data.end())
    return false;

  OrExpr ret;
  for (auto &[src, data] : I->second) {
    ret.add(data.path());
  }
  return ret();
}

expr State::getJumpCond(const BasicBlock &src, const BasicBlock &dst) const {
  auto I = predecessor_data.find(&dst);
  if (I == predecessor_data.end())
    return false;

  auto J = I->second.find(&src);
  return J == I->second.end() ? expr(false)
                              : J->second.path() && *J->second.UB();
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
  inaccessiblemem_bids = src.inaccessiblemem_bids;
  memory.syncWithSrc(src.returnMemory());
}

void State::mkAxioms(State &tgt) {
  assert(isSource() && !tgt.isSource());
  returnMemory().mkAxioms(tgt.returnMemory());
}

}
