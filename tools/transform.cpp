// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include "ir/state.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "util/config.h"
#include "util/errors.h"
#include "util/symexec.h"
#include <map>
#include <set>
#include <sstream>

using namespace IR;
using namespace smt;
using namespace tools;
using namespace util;
using namespace std;


static void print_varval(ostream &s, const Model &m, const Value *var,
                         const StateValue &val) {
  // if the model is partial, we don't know for sure if it's poison or not
  // this happens if the poison constraint depends on an undef
  // however, cexs are usually triggered by the worst case, which is poison
  if (auto v = m.eval(val.non_poison);
      (!v.isConst() || v.isFalse())) {
    s << "poison";
    return;
  }

  if (auto *in = dynamic_cast<const Input*>(var)) {
    uint64_t n;
    ENSURE(m[in->getTyVar()].isUInt(n));
    if (n == 1) {
      s << "undef";
      return;
    }
    assert(n == 0);
  }

  expr e = m.eval(val.value);
  // undef variables may not have a model since each read uses a copy
  if (!e.isConst()) {
    s << "undef";
    return;
  }

  var->getType().printVal(s, e);
}


static void error(Errors &errs, State &src_state, State &tgt_state,
                  const Result &r, bool print_var, const Value *var,
                  const StateValue &src, const StateValue &tgt,
                  const char *msg, bool check_each_var) {

  if (r.isInvalid()) {
    errs.add("Invalid expr");
    return;
  }

  if (r.isUnknown()) {
    errs.add("Timeout");
    return;
  }

  stringstream s;
  string empty;
  auto &var_name = var ? var->getName() : empty;
  auto &m = r.getModel();

  s << msg;
  if (!var_name.empty())
    s << " for " << *var;
  s << "\n\nExample:\n";

  for (auto &[var, val] : src_state.getValues()) {
    if (!dynamic_cast<const Input*>(var))
      continue;
    s << *var << " = ";
    print_varval(s, m, var, val.first);
    s << '\n';
  }

  set<string> seen_vars;
  bool source = true;
  for (auto &vs : { src_state.getValues(), tgt_state.getValues() }) {
    if (!check_each_var) {
      if (source) {
        s << "\nSource:\n";
        source = false;
      } else {
        s << "\nTarget:\n";
      }
    }

    for (auto &[var, val] : vs) {
      auto &name = var->getName();
      if (name == var_name)
        break;

      if (name[0] != '%' ||
          dynamic_cast<const Input*>(var) ||
          (check_each_var && !seen_vars.insert(name).second))
        continue;

      s << *var << " = ";
      print_varval(s, m, var, val.first);
      s << '\n';
    }
  }

  if (print_var) {
    s << "Source value: ";
    print_varval(s, m, var, src);
    s << "\nTarget value: ";
    print_varval(s, m, var, tgt);
  }

  errs.add(s.str());
}


static expr preprocess(Transform &t, const set<expr> &qvars,
                       const set<expr> &undef_qvars, expr && e) {

  // restrict type variable from taking disabled values
  for (auto &i : t.src.getInputs()) {
    auto var = static_cast<const Input &>(i).getTyVar();

    if (config::disable_undef_input)
      e &= var != expr::mkUInt(1, 2);
    if (config::disable_poison_input)
      e &= var.extract(1, 1) == expr::mkUInt(0, 1);
  }

  if (qvars.empty() || e.isFalse())
    return move(e);

  // TODO: maybe try to instantiate undet_xx vars?
  if (undef_qvars.empty() || hit_half_memory_limit())
    return expr::mkForAll(qvars, move(e));

  // manually instantiate all ty_%v vars
  map<expr, expr> instances({ { move(e), true } });
  map<expr, expr> instances2;

  expr nums[3] = { expr::mkUInt(0, 2), expr::mkUInt(1, 2), expr::mkUInt(2, 2) };

  for (auto &i : t.src.getInputs()) {
    auto var = static_cast<const Input&>(i).getTyVar();

    for (auto &[e, v] : instances) {
      for (unsigned i = 0; i <= 2; ++i) {
        expr newexpr = e.subst(var, nums[i]);
        if (newexpr.eq(e)) {
          instances2[move(newexpr)] = v;
          break;
        }

        newexpr = newexpr.simplify();
        if (newexpr.isFalse())
          continue;

        // keep 'var' variables for counterexample printing
        instances2[move(newexpr)] = v && var == nums[i];
      }
    }
    instances = move(instances2);

    // Bail out if it gets too big. It's very likely we can't solve it anyway.
    if (instances.size() >= 128 || hit_half_memory_limit())
      break;
  }

  expr insts(false);
  for (auto &[e, v] : instances) {
    insts |= expr::mkForAll(qvars, move(const_cast<expr&>(e))) && v;
  }

  // TODO: try out instantiating the undefs in forall quantifier

  return insts;
}


static void check_refinement(Errors &errs, Transform &t,
                             State &src_state, State &tgt_state,
                             const Value *var,
                             const expr &dom_a, const State::ValTy &ap,
                             const expr &dom_b, const State::ValTy &bp,
                             bool check_each_var) {
  auto &a = ap.first;
  auto &b = bp.first;

  auto qvars = src_state.getQuantVars();
  qvars.insert(ap.second.begin(), ap.second.end());

  auto err = [&](const Result &r, bool print_var, const char *msg) {
    error(errs, src_state, tgt_state, r, print_var, var, a, b, msg,
          check_each_var);
  };

  expr pre = src_state.getPre() && tgt_state.getPre();

  Solver::check({
    { preprocess(t, qvars, ap.second, pre && dom_a.notImplies(dom_b)),
      [&](const Result &r) {
        err(r, false, "Source is more defined than target");
      }},
    { preprocess(t, qvars, ap.second,
                 pre && dom_a && a.non_poison.notImplies(b.non_poison)),
      [&](const Result &r) {
        err(r, true, "Target is more poisonous than source");
      }},
    { preprocess(t, qvars, ap.second,
                 pre && dom_a && a.non_poison && a.value != b.value),
      [&](const Result &r) {
        err(r, true, "Value mismatch");
      }}
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
  State src_state(t.src, true), tgt_state(t.tgt, false);

  try {
    sym_exec(src_state);
    sym_exec(tgt_state);
  } catch (LoopInCFGDetected&) {
    return "Loops are not supported yet! Skipping function.";
  } catch (OutOfMemory&) {
    return "Out of memory; skipping function.";
  }

  Errors errs;

  if (check_each_var) {
    for (auto &[var, val] : src_state.getValues()) {
      auto &name = var->getName();
      if (name[0] != '%' || !dynamic_cast<const Instr*>(var))
        continue;

      // TODO: add data-flow domain tracking for Alive, but not for TV
      check_refinement(errs, t, src_state, tgt_state, var,
                       true, val, true, tgt_state.at(*tgt_instrs.at(name)),
                       check_each_var);
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
    check_refinement(errs, t, src_state, tgt_state, nullptr,
                     src_state.returnDomain(), src_state.returnVal(),
                     tgt_state.returnDomain(), tgt_state.returnVal(),
                     check_each_var);
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
      auto tgt_i = tgt_inputs.find(i.getName());
      if (tgt_i != tgt_inputs.end())
        c &= i.getType() == tgt_i->second->getType();
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
