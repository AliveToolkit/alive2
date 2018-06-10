// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include "ir/state.h"
#include "smt/expr.h"
#include "smt/solver.h"
#include "util/errors.h"
#include "util/symexec.h"
#include <string>
#include <unordered_map>

using namespace IR;
using namespace smt;
using namespace util;
using namespace std;

static void check_refinement(Solver &s, Errors &errs,
                             const expr &dom_a, const StateValue &a,
                             const expr &dom_b, const StateValue &b) {
  // TODO: handle quantified vars
  // TODO: improve error messages
  s.check({
    { dom_a.notImplies(dom_b), [&](const Model &m) {
      errs.add("Source is more defined than target"); } },

    { dom_a && a.non_poison.notImplies(b.non_poison), [&](const Model &m) {
      errs.add("Target is more poisonous than source"); } },

    { dom_a && a.non_poison && a.value != b.value, [&](const Model &m) {
      errs.add("value mismatch");
     } }
  });
}

namespace tools {

Errors Transform::verify(const TransformVerifyOpts &opts) const {
  Errors errs;

  unordered_map<string, const Value*> tgt_vals;
  for (auto &i : tgt.instrs()) {
    tgt_vals.emplace(i.getName(), &i);
  }

  State src_state(src), tgt_state(tgt);
  sym_exec(src_state);
  sym_exec(tgt_state);

  Solver s;

  if (opts.check_each_var) {
    for (auto &[var, val] : src_state.getValues()) {
      auto &name = var->getName();
      if (name[0] != '%')
        continue;

      // TODO: add data-flow domain tracking for Alive, but not for TV
      check_refinement(s, errs, true, val, true, tgt_state[*tgt_vals.at(name)]);
    }
  }

  if (src_state.fnReturned() != tgt_state.fnReturned()) {
    if (src_state.fnReturned())
      errs.add("Source returns but target doesn't");
    else
      errs.add("Target returns but source doesn't");

  } else if (src_state.fnReturned()) {
    check_refinement(s, errs,
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

TypingAssignments Transform::getTypings() const {
  // TODO: missing cross-program type constraints
  // e.g. for inputs and return values
  return { src.getTypeConstraints() && tgt.getTypeConstraints() };
}

void Transform::fixupTypes(const TypingAssignments &t) {
  src.fixupTypes(t.r.getModel());
  tgt.fixupTypes(t.r.getModel());
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
