// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include "ir/state.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "util/errors.h"
#include "util/symexec.h"

using namespace IR;
using namespace smt;
using namespace tools;
using namespace util;
using namespace std;


static expr preprocess(Transform &t, const set<expr> &qvars,
                       const set<expr> &undef_qvars, expr && e) {
  if (qvars.empty() || e.isFalse())
    return move(e);

  // TODO: maybe try to instantiate undet_xx vars?
  if (undef_qvars.empty() || hit_half_memory_limit())
    return expr::mkForAll(qvars, move(e));

  // manually instantiate all ty_%v vars
  set<expr> instances({ move(e) });
  set<expr> instances2;

  expr nums[3] = { expr::mkUInt(0, 2), expr::mkUInt(1, 2), expr::mkUInt(2, 2) };

  for (auto &i : t.src.getInputs()) {
    auto var = static_cast<const Input&>(i).getTyVar();

    for (auto &e : instances) {
      for (unsigned i = 0; i <= 2; ++i) {
        expr newexpr = e.subst(var, nums[i]);
        if (newexpr.eq(e))
          break;

        newexpr = newexpr.simplify();
        if (newexpr.isFalse())
          continue;
        instances2.emplace(move(newexpr));
      }
    }
    instances = move(instances2);
  }

  expr insts(false);
  for (auto &e : instances) {
    insts |= expr::mkForAll(qvars, move(const_cast<expr&>(e)));
  }

  // TODO: try out instantiating the undefs in forall quantifier

  return insts;
}


static void check_refinement(Errors &errs, Transform &t,
                             const set<expr> &global_qvars,
                             const expr &dom_a, const State::ValTy &ap,
                             const expr &dom_b, const State::ValTy &bp) {
  auto &a = ap.first;
  auto &b = bp.first;

  auto qvars = global_qvars;
  qvars.insert(ap.second.begin(), ap.second.end());

  // TODO: improve error messages
  Solver::check({
    { preprocess(t, qvars, ap.second, dom_a.notImplies(dom_b)),
      [&](const Result &r) { errs.add("Source is more defined than target"); }},

    { preprocess(t, qvars, ap.second,
                 dom_a && a.non_poison.notImplies(b.non_poison)),
      [&](const Result &r) {errs.add("Target is more poisonous than source");}},

    { preprocess(t, qvars, ap.second, dom_a && a.non_poison && a.value != b.value),
      [&](const Result &r) { errs.add("value mismatch"); } }
  });
}

namespace tools {

TransformVerify::TransformVerify(Transform &t, bool check_each_var) :
  t(t), check_each_var(check_each_var) {
  if (check_each_var) {
    for (auto &i : t.tgt.instrs()) {
      tgt_instrs.emplace(i.getName(), &i);
    }
  }
}

Errors TransformVerify::verify() const {
  Value::reset_gbl_id();
  State src_state(t.src), tgt_state(t.tgt);
  sym_exec(src_state);
  sym_exec(tgt_state);

  Errors errs;

  if (check_each_var) {
    for (auto &[var, val] : src_state.getValues()) {
      auto &name = var->getName();
      if (name[0] != '%' || !dynamic_cast<const Instr*>(var))
        continue;

      // TODO: add data-flow domain tracking for Alive, but not for TV
      check_refinement(errs, t, src_state.getQuantVars(),
                       true, val, true, tgt_state.at(*tgt_instrs.at(name)));
      if (errs)
        return errs;
    }
  }

  if (src_state.fnReturned() != tgt_state.fnReturned()) {
    if (src_state.fnReturned())
      errs.add("Source returns but target doesn't");
    else
      errs.add("Target returns but source doesn't");

  } else if (src_state.fnReturned()) {
    check_refinement(errs, t, src_state.getQuantVars(),
                     src_state.returnDomain(), src_state.returnVal(),
                     tgt_state.returnDomain(), tgt_state.returnVal());
  }

  return errs;
}


TypingAssignments::TypingAssignments(const expr &e) {
  if (e.isTrue()) {
    has_only_one_solution = true;
  } else {
    EnableSMTQueriesTMP tmp;
    s.add(e);
    r = s.check();
  }
}

TypingAssignments::operator bool() const {
  return !is_unsat && (has_only_one_solution || r.isSat());
}

void TypingAssignments::operator++(void) {
  if (has_only_one_solution) {
    is_unsat = true;
  } else {
    EnableSMTQueriesTMP tmp;
    s.block(r.getModel());
    r = s.check();
    assert(!r.isUnknown());
  }
}

TypingAssignments TransformVerify::getTypings() const {
  auto c = t.src.getTypeConstraints() && t.tgt.getTypeConstraints();

  // return type
  c &= t.src.getType() == t.tgt.getType();

  // input types
  {
    unordered_map<string, const Value*> tgt_inputs;
    for (auto &i : t.tgt.getInputs()) {
      tgt_inputs.emplace(i.getName(), &i);
    }

    for (auto &i : t.src.getInputs()) {
      c &= i.getType() == tgt_inputs.at(i.getName())->getType();
    }
  }

  if (check_each_var) {
    for (auto &i : t.src.instrs()) {
      c &= i.eqType(*tgt_instrs.at(i.getName()));
    }
  }
  return { move(c) };
}

void TransformVerify::fixupTypes(const TypingAssignments &ty) {
  if (ty.has_only_one_solution)
    return;
  t.src.fixupTypes(ty.r.getModel());
  t.tgt.fixupTypes(ty.r.getModel());
}

void Transform::print(ostream &os, const TransformPrintOpts &opt) const {
  os << "\n----------------------------------------\n";
  if (!name.empty())
    os << "Name: " << name << '\n';
  src.print(os, opt.print_fn_header);
  os << "=>\n";
  tgt.print(os, opt.print_fn_header);
}

ostream& operator<<(ostream &os, const Transform &t) {
  t.print(os, {});
  return os;
}

}
