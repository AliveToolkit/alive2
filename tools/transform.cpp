// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "util/config.h"
#include "util/errors.h"
#include "util/symexec.h"
#include <algorithm>
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

  AndExpr axioms = src_state.getAxioms();
  axioms.add(tgt_state.getAxioms());

  // restrict type variable from taking disabled values
  if (config::disable_undef_input || config::disable_poison_input) {
    for (auto &i : t.src.getInputs()) {
      if (auto in = dynamic_cast<const Input*>(&i)) {
        auto var = in->getTyVar();
        if (config::disable_undef_input) {
          if (config::disable_poison_input)
            axioms.add(var == 0);
          else
            axioms.add(var != 1);
        } else if (config::disable_poison_input)
          axioms.add(var.extract(1, 1) == 0);
      }
    }
  }

  // note that precondition->toSMT() may add stuff to getPre,
  // so order here matters
  // FIXME: broken handling of transformation precondition
  //src_state.startParsingPre();
  //expr pre = t.precondition ? t.precondition->toSMT(src_state) : true;
  expr pre_src = src_state.getPre()();
  expr pre_tgt = tgt_state.getPre()();

  // optimization: rewrite "tgt /\ (src -> foo)" to "tgt /\ foo" if src = tgt
  if (pre_src.eq(pre_tgt))
    pre_src = true;

  auto [poison_cnstr, value_cnstr] = type.refines(src_state, tgt_state, a, b);
  expr memory_cnstr
    = src_state.returnMemory().refined(tgt_state.returnMemory());
  expr axioms_expr = axioms();
  expr dom = dom_a && dom_b;

  if (check_expr(axioms_expr && (pre_src && pre_tgt)).isUnsat()) {
    errs.add("Precondition is always false", false);
    return;
  }

  auto mk_fml = [&](expr &&refines) -> expr {
    // from the check above we already know that
    // \exists v,v' . pre_tgt(v') && pre_src(v) is SAT (or timeout)
    // so \forall v . pre_tgt && (!pre_src(v) || refines) simplifies to:
    // (pre_tgt && !pre_src) || (!pre_src && false) ->   [assume refines=false]
    // \forall v . (pre_tgt && !pre_src(v)) ->  [\exists v . pre_src(v)]
    // false
    if (refines.isFalse())
      return move(refines);

    return axioms_expr &&
             preprocess(t, qvars, uvars, pre_tgt && pre_src.implies(refines));
  };

  Solver::check({
    { mk_fml(dom_a.notImplies(dom_b)),
      [&](const Result &r) {
        err(r, false, "Source is more defined than target");
      }},
    { mk_fml(dom && !poison_cnstr),
      [&](const Result &r) {
        err(r, true, "Target is more poisonous than source");
      }},
    { mk_fml(dom && !value_cnstr),
      [&](const Result &r) {
        err(r, true, "Value mismatch");
      }},
    { mk_fml(dom && !memory_cnstr),
      // FIXME: counterxample is broken (eg should print memory id that differs)
      [&](const Result &r) {
        err(r, true, "Mismatch in memory");
      }}
  });
}

static bool has_nullptr(const Value *v) {
  if (dynamic_cast<const NullPointerValue*>(v) ||
      (dynamic_cast<const UndefValue*>(v) && v->getType().isPtrType()))
      // undef pointer points to the nullblk
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

  // TODO: get this from data layout, varies among address spaces
  bits_size_t = 64;

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
  // Returns access size. 0 if no access, -1 if unknown
  const uint64_t UNKNOWN = -1, NO_ACCESS = 0;
  auto get_access_size = [&](const Instr &inst) -> uint64_t {
    if (dynamic_cast<const Alloc *>(&inst) ||
        dynamic_cast<const Malloc *>(&inst))
      // They are uninitialized (no actual bytes written), so assume that they
      // are not accessed.
      // Calloc is different: it contains memset to zero
      return NO_ACCESS;

    Type *value_ty = nullptr;
    unsigned align = 0;
    if (auto st = dynamic_cast<const Store *>(&inst)) {
      value_ty = &st->getValue()->getType();
      align = st->getAlign();
    } else if (auto ld = dynamic_cast<const Load *>(&inst)) {
      value_ty = &ld->getType();
      align = ld->getAlign();
    }

    if (value_ty) {
      if (auto ity = dynamic_cast<const IntType *>(value_ty))
        return gcd(align, util::divide_up(ity->bits(), 8));
      else if (auto fty = dynamic_cast<const FloatType *>(value_ty))
        return gcd(align, fty->bits() / 8);
      else if (auto pty = dynamic_cast<const PtrType *>(value_ty))
        return gcd(align, bits_size_t / 8);
      // Aggregate types should consider padding
      return UNKNOWN;
    }

    if (dynamic_cast<const BinOp *>(&inst) ||
        dynamic_cast<const UnaryOp *>(&inst) ||
        dynamic_cast<const TernaryOp *>(&inst) ||
        dynamic_cast<const ConversionOp *>(&inst) ||
        dynamic_cast<const Return *>(&inst))
      return NO_ACCESS;

    return UNKNOWN;
  };

  // The number of instructions that can return a pointer to a non-local block.
  num_max_nonlocals_inst = 0;
  // The number of local blocks.
  unsigned num_locals_src = 0, num_locals_tgt = 0;

  for (auto BB : t.src.getBBs()) {
    const auto &instrs = BB->instrs();
    for_each(instrs.begin(), instrs.end(), [&](const auto &i) {
      if (returns_local(i))
        ++num_locals_src;
      else if (returns_nonlocal(i))
        ++num_max_nonlocals_inst;
    });
  }
  for (auto BB : t.tgt.getBBs()) {
    const auto &instrs = BB->instrs();
    for_each(instrs.begin(), instrs.end(), [&](const auto &i) {
      if (returns_local(i))
        ++num_locals_tgt;
      else if (returns_nonlocal(i))
        ++num_max_nonlocals_inst;
    });
  }
  num_locals = max(num_locals_src, num_locals_tgt);

  nullptr_is_used = false;
  has_int2ptr = false;
  has_ptr2int = false;
  // Mininum access size (in bytes)
  uint64_t min_access_size = UNKNOWN;

  for (auto fn : { &t.src, &t.tgt }) {
    for (auto BB : fn->getBBs()) {
      for (auto &I : BB->instrs()) {
        for (auto op : I.operands()) {
          nullptr_is_used |= has_nullptr(op);
        }
        if (auto conv = dynamic_cast<const ConversionOp*>(&I)) {
          has_int2ptr |= conv->getOp() == ConversionOp::Int2Ptr;
          has_ptr2int |= conv->getOp() == ConversionOp::Ptr2Int;
        }

        auto accsz = get_access_size(I);
        if (accsz != NO_ACCESS){
          min_access_size = accsz == UNKNOWN ? 1 :
              (min_access_size == UNKNOWN ? accsz :
                                            gcd(min_access_size, accsz));
        }
      }
    }
  }

  // Include null block
  num_nonlocals = num_globals + num_ptrinputs + num_max_nonlocals_inst + 1;

  // floor(log2(maxblks)) + 1 for local bit
  unsigned maxblks = max(num_locals, num_nonlocals);
  bits_for_bid = (maxblks == 1 ? 1 : ilog2(2 * maxblks - 1)) + 1;

  // TODO
  bits_for_offset = 64;

  // size of byte
  if (num_nonlocals != 1 || t.src.getType().isPtrType())
    // Be conservative, run when there are unescaped local blocks only.
    min_access_size = 1;
  else if (min_access_size >= 1024) {
    // If size is too large, bitvectors explode
    uint64_t i = 1024;
    while (i) {
      if (min_access_size % i == 0) {
        min_access_size = i;
        break;
      }
      i >>= 1;
    }
  }
  bits_byte = 8 * (unsigned)min_access_size;

  little_endian = t.src.isLittleEndian();

  if (config::debug)
    config::dbg() << "num_max_nonlocals_inst: " << num_max_nonlocals_inst << "\n"
                     "num_locals: " << num_locals << "\n"
                     "num_nonlocals: " << num_nonlocals << "\n"
                     "bits_for_bid: " << bits_for_bid << "\n"
                     "bits_for_offset: " << bits_for_offset << "\n"
                     "bits_size_t: " << bits_size_t << "\n"
                     "bits_byte: " << bits_byte << "\n"
                     "little_endian: " << little_endian << "\n"
                     "nullptr_is_used: " << nullptr_is_used << "\n"
                     "has_int2ptr: " << has_int2ptr << "\n"
                     "has_ptr2int: " << has_ptr2int << "\n";
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

  calculateAndInitConstants(t);
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
                   src_state.returnDomain()(), src_state.returnVal(),
                   tgt_state.returnDomain()(), tgt_state.returnVal(),
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
