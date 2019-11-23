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
#include <string_view>

using namespace IR;
using namespace smt;
using namespace tools;
using namespace util;
using namespace std;


static bool is_undef(const expr &e) {
  if (e.isConst())
    return false;
  return check_expr(expr::mkForAll(e.vars(), expr::mkVar("#undef", e) != e)).
           isUnsat();
}

static void print_single_varval(ostream &os, State &st, const Model &m,
                                const Value *var, const Type &type,
                                const StateValue &val) {
  if (!val.isValid()) {
    os << "(invalid expr)";
    return;
  }

  // if the model is partial, we don't know for sure if it's poison or not
  // this happens if the poison constraint depends on an undef
  // however, cexs are usually triggered by the worst case, which is poison
  if (auto v = m.eval(val.non_poison);
      (!v.isConst() || v.isFalse())) {
    os << "poison";
    return;
  }

  if (auto *in = dynamic_cast<const Input*>(var)) {
    uint64_t n;
    ENSURE(m[in->getTyVar()].isUInt(n));
    if (n == 1) {
      os << "undef";
      return;
    }
    assert(n == 0);
  }

  expr partial = m.eval(val.value);
  if (is_undef(partial)) {
    os << "undef";
    return;
  }

  type.printVal(os, st, m.eval(val.value, true));

  // undef variables may not have a model since each read uses a copy
  // TODO: add intervals of possible values for ints at least?
  if (!partial.isConst()) {
    // some functions / vars may not have an interpretation because it's not
    // needed, not because it's undef
    bool found_undef = false;
    for (auto &var : partial.vars()) {
      ostringstream ss;
      ss << var;
      auto name = ss.str();
      found_undef |= string_view(name).substr(0, 6) == "undef!";
    }
    if (found_undef)
      os << "\t[based on undef value]";
  }
}

static void print_varval(ostream &os, State &st, const Model &m,
                         const Value *var, const Type &type,
                         const StateValue &val) {
  if (!type.isAggregateType()) {
    print_single_varval(os, st, m, var, type, val);
    return;
  }

  os << (type.isStructType() ? "{ " : "< ");
  auto agg = type.getAsAggregateType();
  for (unsigned i = 0, e = agg->numElementsConst(); i < e; ++i) {
    if (i != 0)
      os << ", ";
    print_varval(os, st, m, var, agg->getChild(i), agg->extract(val, i));
  }
  os << (type.isStructType() ? " }" : " >");
}


static void error(Errors &errs, State &src_state, State &tgt_state,
                  const Result &r, bool print_var, const Value *var,
                  const Type &type,
                  const StateValue &src, const StateValue &tgt,
                  const char *msg, bool check_each_var) {

  if (r.isInvalid()) {
    errs.add("Invalid expr", false);
    return;
  }

  if (r.isUnknown()) {
    errs.add("Timeout", false);
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

  for (auto &[var, val, used] : src_state.getValues()) {
    (void)used;
    if (!dynamic_cast<const Input*>(var) &&
        !dynamic_cast<const ConstantInput*>(var))
      continue;
    s << *var << " = ";
    print_varval(s, src_state, m, var, var->getType(), val.first);
    s << '\n';
  }

  set<string> seen_vars;
  for (auto st : { &src_state, &tgt_state }) {
    if (!check_each_var) {
      if (st->isSource()) {
        s << "\nSource:\n";
      } else {
        s << "\nTarget:\n";
      }
    }

    for (auto &[var, val, used] : st->getValues()) {
      (void)used;
      auto &name = var->getName();
      if (name == var_name)
        break;

      if (name[0] != '%' ||
          dynamic_cast<const Input*>(var) ||
          (check_each_var && !seen_vars.insert(name).second))
        continue;

      s << *var << " = ";
      print_varval(s, const_cast<State&>(*st), m, var, var->getType(),
                   val.first);
      s << '\n';
    }
  }

  if (print_var) {
    s << "Source value: ";
    print_varval(s, src_state, m, var, type, src);
    s << "\nTarget value: ";
    print_varval(s, tgt_state, m, var, type, tgt);
  }

  errs.add(s.str(), true);
}


static expr preprocess(Transform &t, const set<expr> &qvars,
                       const set<expr> &undef_qvars, expr && e) {
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
    auto in = dynamic_cast<const Input*>(&i);
    if (!in)
      continue;
    auto var = in->getTyVar();

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
                             const Value *var, const Type &type,
                             const expr &dom_a, const State::ValTy &ap,
                             const expr &dom_b, const State::ValTy &bp,
                             bool check_each_var) {
  auto &a = ap.first;
  auto &b = bp.first;

  auto &uvars = ap.second;
  auto qvars = src_state.getQuantVars();
  qvars.insert(ap.second.begin(), ap.second.end());

  auto err = [&](const Result &r, bool print_var, const char *msg) {
    error(errs, src_state, tgt_state, r, print_var, var, type, a, b, msg,
          check_each_var);
  };

  expr axioms = src_state.getAxioms() && tgt_state.getAxioms();

  // restrict type variable from taking disabled values
  if (config::disable_undef_input || config::disable_poison_input) {
    for (auto &i : t.src.getInputs()) {
      if (auto in = dynamic_cast<const Input*>(&i)) {
        auto var = in->getTyVar();
        if (config::disable_undef_input) {
          if (config::disable_poison_input)
            axioms &= var == 0;
          else
            axioms &= var != 1;
        } else if (config::disable_poison_input)
          axioms &= var.extract(1, 1) == 0;
      }
    }
  }

  // note that precondition->toSMT() may add stuff to getPre,
  // so order here matters
  // FIXME: broken handling of transformation precondition
  //src_state.startParsingPre();
  //expr pre = t.precondition ? t.precondition->toSMT(src_state) : true;
  expr pre_src = src_state.getPre();
  expr pre_tgt = tgt_state.getPre();

  auto [poison_cnstr, value_cnstr] = type.refines(src_state, tgt_state, a, b);
  expr memory_cnstr
    = src_state.returnMemory().refined(tgt_state.returnMemory());
  expr dom = dom_a && dom_b;

  Solver::check({
    { axioms && preprocess(t, qvars, uvars,
                           pre_tgt && pre_src.implies(dom_a.notImplies(dom_b))),
      [&](const Result &r) {
        err(r, false, "Source is more defined than target");
      }},
    { axioms && preprocess(t, qvars, uvars,
                           pre_tgt && pre_src.implies(dom && !poison_cnstr)),
      [&](const Result &r) {
        err(r, true, "Target is more poisonous than source");
      }},
    { axioms && preprocess(t, qvars, uvars,
                           pre_tgt && pre_src.implies(dom && !value_cnstr)),
      [&](const Result &r) {
        err(r, true, "Value mismatch");
      }},
    { axioms && preprocess(t, qvars, uvars,
                           pre_tgt && pre_src.implies(dom && !memory_cnstr)),
      [&](const Result &r) {
        err(r, true, "Mismatch in memory");
      }}
  });

  if (!errs && check_expr(axioms && (pre_src && pre_tgt)).isUnsat()) {
    errs.add("Precondition is always false", false);
  }
}

static bool has_nullptr(const Value *v) {
  if (dynamic_cast<const NullPointerValue*>(v))
    return true;

  if (auto agg = dynamic_cast<const AggregateConst*>(v)) {
    for (auto val : agg->getVals()) {
      if (has_nullptr(val))
        return true;
    }
  }

  return false;
}

static void calculateAndInitConstants(Transform &t) {
  const auto &globals_tgt = t.tgt.getGlobalVars();
  const auto &globals_src = t.src.getGlobalVars();
  unsigned num_globals = globals_src.size();

  for (auto GVT : globals_tgt) {
    auto I = find_if(globals_src.begin(), globals_src.end(),
      [GVT](auto *GV) -> bool { return GVT->getName() == GV->getName(); });
    if (I == globals_src.end())
      ++num_globals;
  }

  unsigned num_ptrinputs = 0;
  const auto &inputs = t.src.getInputs();
  for (auto &arg : inputs) {
    // An argument with aggregate type with pointer member isn't regarded
    // as a source of non-local pointer, because its member should be extracted
    // with extractelement / extractvalue instructions, which are also counted
    // as source of non-local blocks.
    num_ptrinputs += arg.getType().isPtrType();
  }

  auto returns_local = [](const Instr &inst) {
    return dynamic_cast<const Alloc *>(&inst) != nullptr ||
           dynamic_cast<const Malloc *>(&inst) != nullptr ||
           dynamic_cast<const Calloc *>(&inst) != nullptr;
  };
  auto returns_nonlocal = [](const Instr &inst) {
    if (!inst.getType().isPtrType())
      return false;
    if (auto conv = dynamic_cast<const ConversionOp *>(&inst)) {
      if (conv->getOp() == ConversionOp::BitCast)
        return false;
    }
    if (dynamic_cast<const GEP *>(&inst))
      return false;
    return true;
  };

  // The number of instructions that can return a pointer to a non-local block.
  unsigned num_inst_nonlocals = 0;
  // The number of local blocks.
  unsigned num_locals_src = 0, num_locals_tgt = 0;
  for (auto BB : t.src.getBBs()) {
    const auto &instrs = BB->instrs();
    for_each(instrs.begin(), instrs.end(), [&](const auto &i) {
      num_locals_src += returns_local(i);
      num_inst_nonlocals += returns_nonlocal(i);
    });
  }
  for (auto BB : t.tgt.getBBs()) {
    const auto &instrs = BB->instrs();
    for_each(instrs.begin(), instrs.end(), [&](const auto &i) {
      num_locals_tgt += returns_local(i);
      num_inst_nonlocals += returns_nonlocal(i);
    });
  }

  bool nullptr_is_used = false;
  for (auto fn : { &t.src, &t.tgt }) {
    for (auto BB : fn->getBBs()) {
      for (auto &I : BB->instrs()) {
        for (auto op : I.operands()) {
          if (has_nullptr(op)) {
            nullptr_is_used = true;
            break;
          }
        }
      }
    }
  }

  if (!nullptr_is_used) {
    for (auto gvs : { &globals_src , &globals_tgt }) {
      for (auto gv : *gvs) {
        if (auto init = gv->initVal()) {
          if (has_nullptr(init)) {
            nullptr_is_used = true;
            break;
          }
        }
      }
    }
  }

  initConstants(num_globals, num_ptrinputs, num_inst_nonlocals,
                max(num_locals_src, num_locals_tgt), nullptr_is_used);
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
  try {
    t.tgt.syncDataWithSrc(t.src);
  } catch (AliveException &ae) {
    return Errors(move(ae));
  }

  // Check sizes of global variables
  auto globals_tgt = t.tgt.getGlobalVars();
  auto globals_src = t.src.getGlobalVars();
  for (auto GVS : globals_src) {
    auto I = find_if(globals_tgt.begin(), globals_tgt.end(),
      [GVS](auto *GV) -> bool { return GVS->getName() == GV->getName(); });
    if (I == globals_tgt.end())
      continue;

    auto GVT = *I;
    if (GVS->size() != GVT->size()) {
      stringstream ss;
      ss << "Unsupported interprocedural transformation: global variable "
        << GVS->getName() << " has different size in source and target ("
        << GVS->size() << " vs " << GVT->size()
        << " bytes)";
      return { ss.str(), false };
    } else if (GVS->isConst() && !GVT->isConst()) {
      stringstream ss;
      ss << "Transformation is incorrect because global variable "
        << GVS->getName() << " is const in source but not in target";
      return { ss.str(), true };
    } else if (!GVS->isConst() && GVT->isConst()) {
      stringstream ss;
      ss << "Unsupported interprocedural transformation: global variable "
        << GVS->getName() << " is const in target but not in source";
      return { ss.str(), false };
    }
  }

  ::calculateAndInitConstants(t);
  State::resetGlobals();
  State src_state(t.src, true), tgt_state(t.tgt, false);

  try {
    sym_exec(src_state);
    tgt_state.copyGlobalVarBidsFromSrc(src_state);
    sym_exec(tgt_state);
  } catch (AliveException e) {
    return move(e);
  }

  Errors errs;

  if (check_each_var) {
    for (auto &[var, val, used] : src_state.getValues()) {
      (void)used;
      auto &name = var->getName();
      if (name[0] != '%' || !dynamic_cast<const Instr*>(var))
        continue;

      // TODO: add data-flow domain tracking for Alive, but not for TV
      check_refinement(errs, t, src_state, tgt_state, var, var->getType(),
                       true, val, true, tgt_state.at(*tgt_instrs.at(name)),
                       check_each_var);
      if (errs)
        return errs;
    }
  }

  check_refinement(errs, t, src_state, tgt_state, nullptr, t.src.getType(),
                   src_state.returnDomain(), src_state.returnVal(),
                   tgt_state.returnDomain(), tgt_state.returnVal(),
                   check_each_var);

  return errs;
}


TypingAssignments::TypingAssignments(const expr &e) : s(true), sneg(true) {
  if (e.isTrue()) {
    has_only_one_solution = true;
  } else {
    EnableSMTQueriesTMP tmp;
    s.add(e);
    sneg.add(!e);
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
    s.block(r.getModel(), &sneg);
    r = s.check();
    assert(!r.isUnknown());
  }
}

TypingAssignments TransformVerify::getTypings() const {
  auto c = t.src.getTypeConstraints() && t.tgt.getTypeConstraints();

  if (t.precondition)
    c &= t.precondition->getTypeConstraints();

  // return type
  c &= t.src.getType() == t.tgt.getType();

  if (check_each_var) {
    for (auto &i : t.src.instrs()) {
      if (!i.isVoid())
        c &= i.eqType(*tgt_instrs.at(i.getName()));
    }
  }
  return { move(c) };
}

void TransformVerify::fixupTypes(const TypingAssignments &ty) {
  if (ty.has_only_one_solution)
    return;
  if (t.precondition)
    t.precondition->fixupTypes(ty.r.getModel());
  t.src.fixupTypes(ty.r.getModel());
  t.tgt.fixupTypes(ty.r.getModel());
}

void Transform::print(ostream &os, const TransformPrintOpts &opt) const {
  os << "\n----------------------------------------\n";
  if (!name.empty())
    os << "Name: " << name << '\n';
  if (precondition) {
    precondition->print(os << "Pre: ");
    os << '\n';
  }
  src.print(os, opt.print_fn_header);
  os << "=>\n";
  tgt.print(os, opt.print_fn_header);
}

ostream& operator<<(ostream &os, const Transform &t) {
  t.print(os, {});
  return os;
}

}
