// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include "ir/state.h"
#include "smt/expr.h"
#include "smt/solver.h"
#include "util/errors.h"
#include "util/symexec.h"

using namespace IR;
using namespace smt;
using namespace util;
using namespace std;

static void check_refinement(Solver &s, Errors &errs,
                             const set<expr> &global_qvars,
                             const expr &dom_a, const State::ValTy &ap,
                             const expr &dom_b, const State::ValTy &bp) {
  auto &a = ap.first;
  auto &b = bp.first;

  auto qvars = global_qvars;
  qvars.insert(ap.second.begin(), ap.second.end());

  // TODO: improve error messages
  s.check({
    { expr::mkForAll(qvars, dom_a.notImplies(dom_b)),
      [&](const Model &m) { errs.add("Source is more defined than target"); } },

    { expr::mkForAll(qvars, dom_a && a.non_poison.notImplies(b.non_poison)),
      [&](const Model &m) {errs.add("Target is more poisonous than source"); }},

    { expr::mkForAll(qvars, dom_a && a.non_poison && a.value != b.value),
      [&](const Model &m) { errs.add("value mismatch"); } }
  });
}

namespace tools {

TransformVerify::TransformVerify(Transform &t, bool check_each_var) :
  t(t), check_each_var(check_each_var) {
  if (check_each_var) {
    for (auto &i : t.tgt.instrs()) {
      tgt_vals.emplace(i.getName(), &i);
    }
  }
}

Errors TransformVerify::verify() const {
  Value::reset_gbl_id();
  State src_state(t.src), tgt_state(t.tgt);
  sym_exec(src_state);
  sym_exec(tgt_state);

  Errors errs;
  Solver s;

  if (check_each_var) {
    for (auto &[var, val] : src_state.getValues()) {
      auto &name = var->getName();
      if (name[0] != '%' || !dynamic_cast<const Instr*>(var))
        continue;

      // TODO: add data-flow domain tracking for Alive, but not for TV
      check_refinement(s, errs, tgt_state.getQuantVars(),
                       true, val, true, tgt_state.at(*tgt_vals.at(name)));
    }
  }

  if (src_state.fnReturned() != tgt_state.fnReturned()) {
    if (src_state.fnReturned())
      errs.add("Source returns but target doesn't");
    else
      errs.add("Target returns but source doesn't");

  } else if (src_state.fnReturned()) {
    check_refinement(s, errs, tgt_state.getQuantVars(),
                     src_state.returnDomain(), src_state.returnVal(),
                     tgt_state.returnDomain(), tgt_state.returnVal());
  }

  return errs;
}


TypingAssignments::TypingAssignments(const expr &e) {
  EnableSMTQueriesTMP tmp;
  s.add(e);
  r = s.check();
}

void TypingAssignments::operator++(void) {
  EnableSMTQueriesTMP tmp;
  s.block(r.getModel());
  r = s.check();
  assert(!r.isUnknown());
}

TypingAssignments TransformVerify::getTypings() const {
  // TODO: missing cross-program type constraints
  // e.g. for inputs and return values
  auto c = t.src.getTypeConstraints() && t.tgt.getTypeConstraints();

  if (check_each_var) {
    for (auto &i : t.src.instrs()) {
      c &= i.getType() == tgt_vals.at(i.getName())->getType();
    }
  }
  return { move(c) };
}

void TransformVerify::fixupTypes(const TypingAssignments &ty) {
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
