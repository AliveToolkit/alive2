// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/transform.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "smt/expr.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "util/config.h"
#include "util/dataflow.h"
#include "util/errors.h"
#include "util/stopwatch.h"
#include "util/symexec.h"
#include <algorithm>
#include <bit>
#include <climits>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>

using namespace IR;
using namespace smt;
using namespace tools;
using namespace util;
using namespace std;
using util::config::dbg;

static void print_single_varval(ostream &os, const State &st, const Model &m,
                                const Value *var, const Type &type,
                                const StateValue &val, unsigned child) {
  if (dynamic_cast<const VoidType*>(&type)) {
    os << "void";
    return;
  }

  if (!val.isValid()) {
    os << "(invalid expr)";
    return;
  }

  // Best effort detection of poison if model is partial
  if (auto v = m.eval(val.non_poison);
      (v.isFalse() || check_expr(!v).isSat())) {
    os << "poison";
    return;
  }

  if (auto *in = dynamic_cast<const Input*>(var)) {
    auto var = in->getUndefVar(type, child);
    if (var.isValid() && m.eval(var, false).isAllOnes()) {
      os << "undef";
      return;
    }
  }

  // TODO: detect undef bits (total or partial) with an SMT query

  expr partial = m.eval(val.value);

  type.printVal(os, st, m, m.eval(val.value, true));

  // undef variables may not have a model since each read uses a copy
  // TODO: add intervals of possible values for ints at least?
  if (!partial.isConst()) {
    // some functions / vars may not have an interpretation because it's not
    // needed, not because it's undef
    for (auto &var : partial.vars()) {
      if (isUndef(var)) {
        os << "\t[based on undef value]";
        break;
      }
    }
  }
}

void tools::print_model_val(ostream &os, const State &st, const Model &m,
                            const Value *var, const Type &type,
                            const StateValue &val, unsigned child) {
  if (!type.isAggregateType()) {
    print_single_varval(os, st, m, var, type, val, child);
    return;
  }

  os << (type.isStructType() ? "{ " : "< ");
  auto agg = type.getAsAggregateType();
  for (unsigned i = 0, e = agg->numElementsConst(); i < e; ++i) {
    if (i != 0)
      os << ", ";
    tools::print_model_val(os, st, m, var, agg->getChild(i),
                           agg->extract(val, i), child + i);
  }
  os << (type.isStructType() ? " }" : " >");
}


using print_var_val_ty = function<void(ostream&, const Model&)>;

static bool error(Errors &errs, const State &src_state, const State &tgt_state,
                  const Result &r, const Value *var, const char *msg,
                  bool check_each_var, print_var_val_ty print_var_val) {

  if (r.isInvalid()) {
    errs.add("Invalid expr", false);
    return true;
  }

  if (r.isTimeout()) {
    errs.add("Timeout", false);
    return false;
  }

  if (r.isError()) {
    errs.add("SMT Error: " + r.getReason(), false);
    return false;
  }

  if (r.isSkip()) {
    errs.add("Skip", false);
    return true;
  }

  stringstream s;
  string empty;
  auto &var_name = var ? var->getName() : empty;
  auto &m = r.getModel();

  {
    // filter out approximations that don't contribute to the bug
    // i.e., they don't show up in the SMT model
    set<string> approx;
    for (auto *v : { &src_state.getApproximations(),
                     &tgt_state.getApproximations() }) {
      for (auto &[msg, var] : *v) {
        if (!var || m.hasFnModel(*var))
          approx.emplace(msg);
      }
    }

    if (!approx.empty()) {
      s << "Couldn't prove the correctness of the transformation\n"
          "Alive2 approximated the semantics of the programs and therefore we\n"
          "cannot conclude whether the bug found is valid or not.\n\n"
          "Approximations done:\n";
      for (auto &msg : approx) {
        s << " - " << msg << '\n';
      }
      errs.add(s.str(), false);
      return false;
    }
  }

  s << msg;
  if (!var_name.empty())
    s << " for " << *var;
  s << "\n\nExample:\n";

  for (auto &[var, val] : src_state.getValues()) {
    if (!dynamic_cast<const Input*>(var) &&
        !dynamic_cast<const ConstantInput*>(var))
      continue;
    s << *var << " = ";
    print_model_val(s, src_state, m, var, var->getType(), val.val);
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

    auto *bb = &st->getFn().getFirstBB();

    for (auto &[var, val] : st->getValues()) {
      auto &name = var->getName();
      if (name == var_name)
        break;

      auto *i = dynamic_cast<const Instr*>(var);
      if (!i || &st->getFn().bbOf(*i) != bb ||
          (check_each_var && !seen_vars.insert(name).second))
        continue;

      if (auto *jmp = dynamic_cast<const JumpInstr*>(var)) {
        bool jumped = false;
        for (auto &dst : jmp->targets()) {
          const expr &cond = st->getJumpCond(*bb, dst);
          if ((jumped = !m.eval(cond).isFalse())) {
            s << "  >> Jump to " << dst.getName() << '\n';
            bb = &dst;
            break;
          }
        }

        if (jumped) {
          continue;
        } else {
          s << "UB triggered on " << name << '\n';
          break;
        }
      }

      if (auto call = dynamic_cast<const FnCall*>(var)) {
        if (m.eval(val.return_domain).isFalse()) {
          s << *var << " = function did not return!\n";
          break;
        } else if (var->isVoid()) {
          s << "Function " << call->getFnName() << " returned\n";
          continue;
        }
      }

      if (!dynamic_cast<const Return*>(var) && // domain always false after exec
          !m.eval(val.domain).isTrue()) {
        s << *var << " = UB triggered!\n";
        break;
      }

      if (name[0] != '%')
        continue;

      s << *var << " = ";
      print_model_val(s, const_cast<State&>(*st), m, var, var->getType(),
                      val.val);
      s << '\n';
    }

    st->getMemory().print(s, m);
  }

  print_var_val(s, m);
  errs.add(s.str(), true);
  return false;
}


static void instantiate_undef(const Input *in, map<expr, expr> &instances,
                              const Type &ty, unsigned child) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i < e; ++i) {
      if (!agg->isPadding(i))
        instantiate_undef(in, instances, agg->getChild(i), child + i);
    }
    return;
  }

  // Bail out if it gets too big. It's unlikely we can solve it anyway.
  if (instances.size() >= 128 || hit_half_memory_limit())
    return;

  auto var = in->getUndefVar(ty, child);
  if (!var.isValid())
    return;

  // TODO: add support for per-bit input undef
  assert(var.bits() == 1);

  map<expr, expr> instances2;
  expr nums[2] = { expr::mkUInt(0, 1), expr::mkUInt(1, 1) };

  for (auto I = instances.begin(); I != instances.end();
       I = instances.erase(I)) {

    if (hit_half_memory_limit()) {
      instances2.insert(instances.begin(), instances.end());
      break;
    }

    auto &[e, v] = *I;
    for (unsigned i = 0; i < 2; ++i) {
      expr newexpr = e.subst(var, nums[i]);
      if (newexpr.eq(e)) {
        instances2[std::move(newexpr)] = std::move(v);
        break;
      }

      newexpr = newexpr.simplify();
      if (newexpr.isFalse())
        continue;

      // keep 'var' variables for counterexample printing
      instances2.try_emplace(std::move(newexpr), v && var == nums[i]);
    }
  }
  instances = std::move(instances2);
}

static expr preprocess(const Transform &t, const set<expr> &qvars0,
                       const set<expr> &undef_qvars, expr &&e) {
  if (hit_half_memory_limit())
    return expr::mkForAll(qvars0, std::move(e));

  // eliminate all quantified boolean vars; Z3 gets too slow with those
  auto qvars = qvars0;
  unsigned num_qvars_subst = 0;
  for (auto I = qvars.begin(); I != qvars.end(); ) {
    auto &var = *I;
    if (!var.isBool()) {
      ++I;
      continue;
    }
    if (hit_half_memory_limit())
      break;

    e = (e.subst(var, true) && e.subst(var, false)).simplify();
    I = qvars.erase(I);

    // Z3's subst is *super* slow; avoid exponential run-time
    if (++num_qvars_subst == 5)
      break;
  }

  if (config::disable_undef_input || undef_qvars.empty() ||
      hit_half_memory_limit())
    return expr::mkForAll(qvars, std::move(e));

  // manually instantiate undef masks
  map<expr, expr> instances({ { std::move(e), true } });

  for (auto &i : t.src.getInputs()) {
    if (auto in = dynamic_cast<const Input*>(&i))
      instantiate_undef(in, instances, i.getType(), 0);
  }

  expr insts(false);
  for (auto &[e, v] : instances) {
    insts |= expr::mkForAll(qvars, std::move(const_cast<expr&>(e))) && v;
  }
  return insts;
}


static expr
encode_undef_refinement_per_elem(const Type &ty, const StateValue &sva,
                                 expr &&a2, expr &&b, expr &&b2) {
  const auto *aty = ty.getAsAggregateType();
  if (!aty)
    return sva.non_poison && sva.value == a2 && b != b2;

  StateValue sva2{std::move(a2), expr()};
  StateValue svb{std::move(b), expr()}, svb2{std::move(b2), expr()};
  expr result = false;

  for (unsigned i = 0; i < aty->numElementsConst(); ++i) {
    if (!aty->isPadding(i))
      result |= encode_undef_refinement_per_elem(aty->getChild(i),
                  aty->extract(sva, i), aty->extract(sva2, i).value,
                  aty->extract(svb, i).value, aty->extract(svb2, i).value);
  }
  return result;
}

// Returns negation of refinement
static expr encode_undef_refinement(const Type &type, const State::ValTy &a,
                                    const State::ValTy &b) {
  // Undef refinement: (src-nonpoison /\ src-nonundef) -> tgt-nonundef
  //
  // Full refinement formula:
  //   forall I .
  //    (forall N . src_nonpoison(I, N) /\ retval_src(I, N) == retval_src(I, 0))
  //      -> (forall N . retval_tgt(I, N) == retval_tgt(I, 0)

  if (dynamic_cast<const VoidType *>(&type))
    return false;
  if (b.undef_vars.empty())
    // target is never undef
    return false;

  auto subst = [](const auto &val) {
    vector<pair<expr, expr>> repls;
    for (auto &v : val.undef_vars) {
      repls.emplace_back(v, expr::some(v));
    }
    return val.val.value.subst(repls);
  };

  return encode_undef_refinement_per_elem(type, a.val, subst(a),
                                          expr(b.val.value), subst(b));
}

static void
check_refinement(Errors &errs, const Transform &t, State &src_state,
                 State &tgt_state, const Value *var, const Type &type,
                 const State::ValTy &ap, const State::ValTy &bp,
                 bool check_each_var) {
  auto &fndom_a  = ap.domain;
  auto &fndom_b  = bp.domain;
  auto &retdom_a = ap.return_domain;
  auto &retdom_b = bp.return_domain;
  auto &a = ap.val;
  auto &b = bp.val;

  auto &uvars = ap.undef_vars;
  auto qvars = src_state.getQuantVars();
  qvars.insert(ap.undef_vars.begin(), ap.undef_vars.end());
  auto &fn_qvars = tgt_state.getFnQuantVars();
  qvars.insert(fn_qvars.begin(), fn_qvars.end());

  AndExpr axioms = src_state.getAxioms();
  axioms.add(tgt_state.getAxioms());
  expr axioms_expr = axioms();

  // note that precondition->toSMT() may add stuff to getPre,
  // so order here matters
  // FIXME: broken handling of transformation precondition
  //src_state.startParsingPre();
  //expr pre = t.precondition ? t.precondition->toSMT(src_state) : true;
  auto pre_src_and = src_state.getPre();
  auto &pre_tgt_and = tgt_state.getPre();

  // optimization: rewrite "tgt /\ (src -> foo)" to "tgt /\ foo" if src = tgt
  pre_src_and.del(pre_tgt_and);
  expr pre_src = pre_src_and();
  expr pre_tgt = pre_tgt_and();

  if (check_expr(axioms_expr && (pre_src && pre_tgt)).isUnsat()) {
    errs.add("Precondition is always false", false);
    return;
  }

  {
    auto sink_src = src_state.sinkDomain();
    if (!sink_src.isTrue() && check_expr(axioms_expr && !sink_src).isUnsat()) {
      errs.add("The source program doesn't reach a return instruction.\n"
               "Consider increasing the unroll factor if it has loops", false);
      return;
    }

    auto sink_tgt = tgt_state.sinkDomain();
    if (!sink_tgt.isTrue() && check_expr(axioms_expr && !sink_tgt).isUnsat()) {
      errs.add("The target program doesn't reach a return instruction.\n"
               "Consider increasing the unroll factor if it has loops", false);
      return;
    }

    pre_tgt &= !sink_tgt;
  }

  expr pre_src_exists, pre_src_forall;
  {
    vector<pair<expr,expr>> repls;
    auto vars_pre = pre_src.vars();
    for (auto &v : qvars) {
      if (vars_pre.count(v))
        repls.emplace_back(v, expr::mkFreshVar("#exists", v));
    }
    pre_src_exists = pre_src.subst(repls);
    pre_src_forall = pre_src_exists.eq(pre_src) ? true : pre_src;
  }
  expr pre = pre_src_exists && pre_tgt && src_state.getFnPre();
  pre_src_forall &= tgt_state.getFnPre();

  auto mk_fml = [&](expr &&refines) -> expr {
    // from the check above we already know that
    // \exists v,v' . pre_tgt(v') && pre_src(v) is SAT (or timeout)
    // so \forall v . pre_tgt && (!pre_src(v) || refines) simplifies to:
    // (pre_tgt && !pre_src) || (!pre_src && false) ->   [assume refines=false]
    // \forall v . (pre_tgt && !pre_src(v)) ->  [\exists v . pre_src(v)]
    // false
    if (refines.isFalse())
      return std::move(refines);

    return axioms_expr &&
            preprocess(t, qvars, uvars, pre && pre_src_forall.implies(refines));
  };

  auto check = [&](expr &&e, auto &&printer, const char *msg) {
    e = mk_fml(std::move(e));
    auto res = check_expr(e);
    if (!res.isUnsat() &&
        !error(errs, src_state, tgt_state, res, var, msg, check_each_var,
               printer))
      return false;
    return true;
  };

#define CHECK(fml, printer, msg) \
  if (!check(fml, printer, msg)) \
    return

  // 1. Check UB
  CHECK(fndom_a.notImplies(fndom_b),
        [](ostream&, const Model&){}, "Source is more defined than target");

  // 2. Check return domain (noreturn check)
  {
    expr dom_constr;
    if (retdom_a.eq(fndom_a) && retdom_b.eq(fndom_b)) { // A /\ B /\ A != B
      dom_constr = false;
    } else {
      dom_constr = (fndom_a && fndom_b) && retdom_a != retdom_b;
    }

    CHECK(std::move(dom_constr),
          [](ostream&, const Model&){},
          "Source and target don't have the same return domain");
  }

  // 3. Check poison
  auto print_value = [&](ostream &s, const Model &m) {
    s << "Source value: ";
    print_model_val(s, src_state, m, var, type, a);
    s << "\nTarget value: ";
    print_model_val(s, tgt_state, m, var, type, b);
  };

  auto [poison_cnstr, value_cnstr] = type.refines(src_state, tgt_state, a, b);
  expr dom = retdom_a && retdom_b;
  if (check_each_var)
    dom &= fndom_a && fndom_b;

  CHECK(dom && !poison_cnstr,
        print_value, "Target is more poisonous than source");

  // 4. Check undef
  CHECK(dom && encode_undef_refinement(type, ap, bp),
        print_value, "Target's return value is more undefined");

  // 5. Check value
  CHECK(dom && !value_cnstr, print_value, "Value mismatch");

  // 6. Check memory
  auto &src_mem = src_state.returnMemory();
  auto &tgt_mem = tgt_state.returnMemory();
  auto [memory_cnstr0, ptr_refinement0, mem_undef]
    = src_mem.refined(tgt_mem, false);
  auto &ptr_refinement = ptr_refinement0;
  qvars.insert(mem_undef.begin(), mem_undef.end());

  auto print_ptr_load = [&](ostream &s, const Model &m) {
    set<expr> undef;
    Pointer p(src_mem, m[ptr_refinement()]);
    s << "\nMismatch in " << p
      << "\nSource value: " << Byte(src_mem, m[src_mem.raw_load(p, undef)()])
      << "\nTarget value: " << Byte(tgt_mem, m[tgt_mem.raw_load(p, undef)()]);
  };

  CHECK(dom && !(memory_cnstr0.isTrue() ? memory_cnstr0
                                        : value_cnstr && memory_cnstr0),
        print_ptr_load, "Mismatch in memory");

#undef CHECK
}

static bool has_nullptr(const Value *v) {
  if (dynamic_cast<const NullPointerValue*>(v) ||
      (dynamic_cast<const UndefValue*>(v) && hasPtr(v->getType())))
      // undef pointer points to the nullblk
    return true;

  if (auto agg = dynamic_cast<const AggregateValue*>(v)) {
    for (auto val : agg->getVals()) {
      if (has_nullptr(val))
        return true;
    }
  }

  return false;
}

static unsigned num_ptrs(const Type &ty) {
  unsigned n = ty.isPtrType();
  if (auto aty = ty.getAsAggregateType())
    n += aty->numPointerElements();
  return n;
}

static bool returns_local(const Value &v) {
  // no alias fns return local block
  if (auto call = dynamic_cast<const FnCall*>(&v))
    return call->hasAttribute(FnAttrs::NoAlias) ||
           call->getAttributes().isAlloc();

  return dynamic_cast<const Alloc*>(&v);
}

static Value *get_base_ptr(Value *ptr) {
  vector<Value*> todo = { ptr };
  Value *base_ptr = nullptr;
  set<Value*> seen;
  do {
    ptr = todo.back();
    todo.pop_back();
    if (!seen.insert(ptr).second)
      continue;

    if (auto gep = dynamic_cast<GEP*>(ptr)) {
      todo.emplace_back(&gep->getPtr());
      continue;
    }

    if (auto c = isNoOp(*ptr)) {
      todo.emplace_back(c);
      continue;
    }

    if (auto phi = dynamic_cast<Phi*>(ptr)) {
      auto ops = phi->operands();
      todo.insert(todo.end(), ops.begin(), ops.end());
      continue;
    }

    if (auto s = dynamic_cast<Select*>(ptr)) {
      todo.emplace_back(s->getTrueValue());
      todo.emplace_back(s->getFalseValue());
      continue;
    }

    if (base_ptr && base_ptr != ptr)
      return nullptr;

    base_ptr = ptr;

  } while (!todo.empty());

  return base_ptr;
}

static bool may_be_nonlocal(Value *ptr) {
  vector<Value*> todo = { ptr };
  set<Value*> seen;
  do {
    ptr = todo.back();
    todo.pop_back();
    if (!seen.insert(ptr).second)
      continue;

    if (returns_local(*ptr))
      continue;

    if (auto gep = dynamic_cast<GEP*>(ptr)) {
      todo.emplace_back(&gep->getPtr());
      continue;
    }

    if (auto c = isNoOp(*ptr)) {
      todo.emplace_back(c);
      continue;
    }

    if (auto phi = dynamic_cast<Phi*>(ptr)) {
      auto ops = phi->operands();
      todo.insert(todo.end(), ops.begin(), ops.end());
      continue;
    }

    if (auto s = dynamic_cast<Select*>(ptr)) {
      todo.emplace_back(s->getTrueValue());
      todo.emplace_back(s->getFalseValue());
      continue;
    }
    return true;

  } while (!todo.empty());

  return false;
}

static pair<Value*, uint64_t> collect_gep_offsets(Value &v) {
  Value *ptr = &v;
  uint64_t offset = 0;

  while (true) {
    if (auto gep = dynamic_cast<GEP*>(ptr)) {
      uint64_t off = gep->getMaxGEPOffset();
      if (off != UINT64_MAX) {
        ptr = &gep->getPtr();
        offset += off;
        continue;
      }
    }
    break;
  }

  return { ptr, offset };
}

static unsigned returns_nonlocal(const Instr &inst,
                                 set<pair<Value*, uint64_t>> &cache) {
  bool rets_nonloc = false;

  if (dynamic_cast<const FnCall*>(&inst) ||
      isCast(ConversionOp::Int2Ptr, inst)) {
    rets_nonloc = true;
  }
  else if (auto load = dynamic_cast<const Load *>(&inst)) {
    if (may_be_nonlocal(&load->getPtr())) {
      rets_nonloc = cache.emplace(collect_gep_offsets(load->getPtr())).second;
    }
  }
  return rets_nonloc ? num_ptrs(inst.getType()) : 0;
}

namespace {
struct CountMemBlock {
  unsigned num_nonlocals = 0;
  set<pair<Value*, uint64_t>> nonlocal_cache;

  void exec(const Instr &i, CountMemBlock &glb_data) {
    if (returns_local(i)) {
      // TODO: can't be path sensitive yet
    } else {
      num_nonlocals += returns_nonlocal(i, nonlocal_cache);
      glb_data.num_nonlocals += returns_nonlocal(i, glb_data.nonlocal_cache);
      num_nonlocals = min(num_nonlocals, glb_data.num_nonlocals);
    }
  }

  void merge(const CountMemBlock &other) {
    // if LHS has x more non-locals than RHS, then it gets to keep the first
    // x cached accessed pointers, as for sure we have accessed all common
    // pointers plus x extra pointers if we go through the LHS path
    unsigned delta = num_nonlocals - other.num_nonlocals;
    bool lhs_larger = num_nonlocals >= other.num_nonlocals;
    if (!lhs_larger) {
      num_nonlocals = other.num_nonlocals;
      delta = -delta;
    }

    for (auto I = nonlocal_cache.begin(); I != nonlocal_cache.end(); ) {
      if (other.nonlocal_cache.count(*I)) {
        ++I;
      } else if (delta > 0) {
        ++I;
        --delta;
      } else {
        I = nonlocal_cache.erase(I);
      }
    }

    for (auto &e : other.nonlocal_cache) {
      if (delta > 0 && nonlocal_cache.emplace(e).second) {
        --delta;
      }
    }
  }
};
}


static void initBitsProgramPointer(Transform &t) {
  // FIXME: varies among address spaces
  bits_program_pointer = t.src.bitsPointers();
  assert(bits_program_pointer > 0 && bits_program_pointer <= 64);
  assert(bits_program_pointer == t.tgt.bitsPointers());
}

static uint64_t aligned_alloc_size(uint64_t size, unsigned align) {
  if (size <= align)
    return align;
  return add_saturate(size, align - 1);
}

static void calculateAndInitConstants(Transform &t) {
  if (!bits_program_pointer)
    initBitsProgramPointer(t);

  const auto &globals_tgt = t.tgt.getGlobalVars();
  const auto &globals_src = t.src.getGlobalVars();
  num_globals_src = globals_src.size();
  unsigned num_globals = num_globals_src;
  uint64_t glb_alloc_aligned_size = 0;

  heap_block_alignment = 8;

  num_consts_src = 0;

  for (auto GV : globals_src) {
    if (GV->isConst())
      ++num_consts_src;
    glb_alloc_aligned_size
      = add_saturate(glb_alloc_aligned_size,
                     aligned_alloc_size(GV->size(), GV->getAlignment()));
  }

  for (auto GVT : globals_tgt) {
    auto I = find_if(globals_src.begin(), globals_src.end(),
      [GVT](auto *GV) -> bool { return GVT->getName() == GV->getName(); });
    if (I == globals_src.end()) {
      ++num_globals;
    }
    glb_alloc_aligned_size
      = add_saturate(glb_alloc_aligned_size,
                     aligned_alloc_size(GVT->size(), GVT->getAlignment()));
  }

  num_ptrinputs = 0;
  unsigned num_null_ptrinputs = 0;
  for (auto &arg : t.src.getInputs()) {
    auto n = num_ptrs(arg.getType());
    auto in = dynamic_cast<const Input*>(&arg);
    if (in && in->hasAttribute(ParamAttrs::ByVal)) {
      num_globals_src += n;
      num_globals += n;
    } else {
      num_ptrinputs += n;
      if (!in || !in->hasAttribute(ParamAttrs::NonNull))
        num_null_ptrinputs += n;
    }
  }

  // The number of local blocks.
  num_locals_src = 0;
  num_locals_tgt = 0;
  uint64_t max_gep_src = 0, max_gep_tgt = 0;
  uint64_t max_alloc_size = 0;
  uint64_t max_access_size = 0;
  uint64_t min_global_size = UINT64_MAX;

  bool has_null_pointer = false;
  bool has_int2ptr     = false;
  bool has_ptr2int     = false;
  has_alloca       = false;
  has_fncall       = false;
  has_write_fncall = false;
  has_null_block   = false;
  does_ptr_store   = false;
  does_ptr_mem_access = false;
  does_int_mem_access = false;
  observes_addresses  = false;
  bool does_any_byte_access = false;

  set<string> inaccessiblememonly_fns;
  num_inaccessiblememonly_fns = 0;

  // Mininum access size (in bytes)
  uint64_t min_access_size = 8;
  uint64_t loc_src_alloc_aligned_size = 0;
  uint64_t loc_tgt_alloc_aligned_size = 0;
  unsigned min_vect_elem_sz = 0;
  bool does_mem_access = false;
  bool has_ptr_load = false;

  auto update_min_vect_sz = [&](const Type &ty) {
    auto elemsz = minVectorElemSize(ty);
    if (min_vect_elem_sz && elemsz)
      min_vect_elem_sz = gcd(min_vect_elem_sz, elemsz);
    else if (elemsz)
      min_vect_elem_sz = elemsz;
  };

  for (auto fn : { &t.src, &t.tgt }) {
    bool is_src = fn == &t.src;
    unsigned &cur_num_locals = is_src ? num_locals_src : num_locals_tgt;
    uint64_t &cur_max_gep    = is_src ? max_gep_src : max_gep_tgt;
    uint64_t &loc_alloc_aligned_size
      = is_src ? loc_src_alloc_aligned_size : loc_tgt_alloc_aligned_size;

    for (auto &v : fn->getInputs()) {
      auto *i = dynamic_cast<const Input *>(&v);
      if (!i)
        continue;

      update_min_vect_sz(i->getType());

      if (i->hasAttribute(ParamAttrs::Dereferenceable)) {
        does_mem_access = true;
        uint64_t deref_bytes = i->getAttributes().derefBytes;
        max_access_size = max(max_access_size, deref_bytes);
      }
      if (i->hasAttribute(ParamAttrs::DereferenceableOrNull)) {
        // Optimization: unless explicitly compared with a null pointer, don't
        // set has_null_pointer to true.
        // Null constant pointer will set has_null_pointer to true anyway.
        // Note that dereferenceable_or_null implies num_ptrinputs > 0,
        // which may turn has_null_block on.
        does_mem_access = true;
        uint64_t deref_bytes = i->getAttributes().derefOrNullBytes;
        max_access_size = max(max_access_size, deref_bytes);
      }
      if (i->hasAttribute(ParamAttrs::ByVal)) {
        does_mem_access = true;
        uint64_t sz = i->getAttributes().blockSize;
        max_access_size = max(max_access_size, sz);
        min_global_size = min_global_size != UINT64_MAX
                            ? gcd(sz, min_global_size)
                            : sz;
      }
    }

    for (auto &i : fn->instrs()) {
      if (returns_local(i))
        ++cur_num_locals;

      for (auto op : i.operands()) {
        has_null_pointer |= has_nullptr(op);
        update_min_vect_sz(op->getType());
      }

      update_min_vect_sz(i.getType());

      if (auto fn = dynamic_cast<const FnCall*>(&i)) {
        has_fncall |= true;
        if (!fn->getAttributes().isAlloc()) {
          if (fn->getAttributes().mem.canOnlyWrite(MemoryAccess::Inaccessible)) {
            if (inaccessiblememonly_fns.emplace(fn->getName()).second)
              ++num_inaccessiblememonly_fns;
          } else {
            if (fn->getAttributes().mem
                                   .canOnlyRead(MemoryAccess::Inaccessible)) {
              if (inaccessiblememonly_fns.emplace(fn->getName()).second)
                ++num_inaccessiblememonly_fns;
            }
            has_write_fncall |= fn->getAttributes().mem.canWriteSomething();
          }
        }
      }

      if (auto *mi = dynamic_cast<const MemInstr *>(&i)) {
        auto [alloc, align] = mi->getMaxAllocSize();
        max_alloc_size     = max(max_alloc_size, alloc);
        loc_alloc_aligned_size = add_saturate(loc_alloc_aligned_size,
                                              aligned_alloc_size(alloc, align));
        max_access_size  = max(max_access_size, mi->getMaxAccessSize());
        cur_max_gep      = add_saturate(cur_max_gep, mi->getMaxGEPOffset());

        auto info = mi->getByteAccessInfo();
        has_ptr_load         |= info.doesPtrLoad;
        does_ptr_store       |= info.doesPtrStore;
        does_int_mem_access  |= info.hasIntByteAccess;
        does_mem_access      |= info.doesMemAccess();
        observes_addresses   |= info.observesAddresses;
        min_access_size       = gcd(min_access_size, info.byteSize);
        if (info.doesMemAccess() && !info.hasIntByteAccess &&
            !info.doesPtrLoad && !info.doesPtrStore)
          does_any_byte_access = true;

        has_alloca |= dynamic_cast<const Alloc*>(&i) != nullptr;

      } else if (isCast(ConversionOp::Int2Ptr, i) ||
                  isCast(ConversionOp::Ptr2Int, i)) {
        max_alloc_size = max_access_size = cur_max_gep = loc_alloc_aligned_size
          = UINT64_MAX;
        has_int2ptr |= isCast(ConversionOp::Int2Ptr, i) != nullptr;
        has_ptr2int |= isCast(ConversionOp::Ptr2Int, i) != nullptr;

      } else if (auto *bc = isCast(ConversionOp::BitCast, i)) {
        auto &t = bc->getType();
        min_access_size = gcd(min_access_size, getCommonAccessSize(t));

      } else if (auto *ic = dynamic_cast<const ICmp*>(&i)) {
        observes_addresses |= ic->isPtrCmp() &&
                              ic->getPtrCmpMode() == ICmp::INTEGRAL;
      }
    }
  }

  unsigned num_nonlocals_inst_src;
  {
    DenseDataFlow<CountMemBlock> df(t.src);
    num_nonlocals_inst_src = df.getResult().num_nonlocals;
  }

  does_ptr_mem_access = has_ptr_load || does_ptr_store;
  if (does_any_byte_access && !does_int_mem_access && !does_ptr_mem_access)
    // Use int bytes only
    does_int_mem_access = true;

  unsigned num_locals = max(num_locals_src, num_locals_tgt);

  for (auto glbs : { &globals_src, &globals_tgt }) {
    for (auto &glb : *glbs) {
      auto sz = max(glb->size(), (uint64_t)1u);
      max_access_size = max(sz, max_access_size);
      min_global_size = min_global_size != UINT64_MAX
                          ? gcd(sz, min_global_size)
                          : sz;
    }
  }

  // check if null block is needed
  // Global variables cannot be null pointers
  has_null_block = num_null_ptrinputs > 0 || has_null_pointer ||
                  has_ptr_load || has_fncall || has_int2ptr;

  num_nonlocals_src = num_globals_src + num_ptrinputs + num_nonlocals_inst_src +
                      num_inaccessiblememonly_fns + has_null_block;

  null_is_dereferenceable = t.src.getFnAttrs().has(FnAttrs::NullPointerIsValid);

  // Allow at least one non-const global for calls to change
  num_nonlocals_src += has_write_fncall;

  num_nonlocals = num_nonlocals_src + num_globals - num_globals_src;

  observes_addresses |= has_int2ptr || has_ptr2int;
  // condition can happen with ptr2int(poison)
  if (has_ptr2int && num_nonlocals == 0) {
    ++num_nonlocals_src;
    ++num_nonlocals;
  }

  if (!does_int_mem_access && !does_ptr_mem_access && has_fncall)
    does_int_mem_access = true;

  auto has_attr = [&](ParamAttrs::Attribute a) -> bool {
    for (auto fn : { &t.src, &t.tgt }) {
      for (auto &v : fn->getInputs()) {
        auto i = dynamic_cast<const Input*>(&v);
        if (i && i->hasAttribute(a))
          return true;
      }
    }
    return false;
  };
  // The number of bits needed to encode pointer attributes
  // nonnull and byval isn't encoded in ptr attribute bits
  has_nocapture = has_attr(ParamAttrs::NoCapture);
  has_noread = has_attr(ParamAttrs::NoRead);
  has_nowrite = has_attr(ParamAttrs::NoWrite);
  bits_for_ptrattrs = has_nocapture + has_noread + has_nowrite;

  // ceil(log2(maxblks)) + 1 for local bit
  bits_for_bid = max(1u, ilog2_ceil(max(num_locals, num_nonlocals), false))
                   + (num_locals && num_nonlocals);

  // reserve a multiple of 4 for the number of offset bits to make SMT &
  // counterexamples more readable
  // Allow an extra bit for the sign
  auto max_geps
    = bit_width(add_saturate(max(max_gep_src, max_gep_tgt), max_access_size))
        + 1;
  bits_for_offset = min(round_up(max_geps, 4), (uint64_t)t.src.bitsPtrOffset());
  bits_for_offset = min(bits_for_offset, config::max_offset_bits);
  bits_for_offset = min(bits_for_offset, bits_program_pointer);

  // ASSUMPTION: programs can only allocate up to half of address space
  // so the first bit of size is always zero.
  // We need this assumption to support negative offsets.
  bits_size_t = bit_width(max_alloc_size);
  bits_size_t = min(max(bits_for_offset, bits_size_t), bits_program_pointer-1);
  bits_size_t = min(bits_size_t, config::max_sizet_bits);

  // +1 because the pointer after the object must be valid (can't overflow)
  uint64_t loc_alloc_aligned_size
    = max(loc_src_alloc_aligned_size, loc_tgt_alloc_aligned_size);
  bits_ptr_address
    = add_saturate(
        bit_width(max(glb_alloc_aligned_size, loc_alloc_aligned_size)), 1);

  // as an (unsound) optimization, we fix the first bit of the addr for
  // local/non-local if both exist (to reduce axiom fml size)
  bool has_local_bit = (num_locals_src || num_locals_tgt) && num_nonlocals;
  bits_ptr_address = min(max(bits_size_t, bits_ptr_address) + has_local_bit,
                         bits_program_pointer);

  bits_byte = 8 * (does_mem_access ?  (unsigned)min_access_size : 1);

  bits_poison_per_byte = 1;
  if (min_vect_elem_sz > 0)
    bits_poison_per_byte = (min_vect_elem_sz % 8) ? bits_byte :
                             bits_byte / gcd(bits_byte, min_vect_elem_sz);

  strlen_unroll_cnt = 10;
  memcmp_unroll_cnt = 10;

  little_endian = t.src.isLittleEndian();

  if (config::debug)
    config::dbg() << "\nnum_locals_src: " << num_locals_src
                  << "\nnum_locals_tgt: " << num_locals_tgt
                  << "\nnum_nonlocals_src: " << num_nonlocals_src
                  << "\nnum_nonlocals: " << num_nonlocals
                  << "\nnum_inaccessiblememonly_fns: "
                    << num_inaccessiblememonly_fns
                  << "\nbits_for_bid: " << bits_for_bid
                  << "\nbits_for_offset: " << bits_for_offset
                  << "\nbits_size_t: " << bits_size_t
                  << "\nbits_ptr_address: " << bits_ptr_address
                  << "\nbits_program_pointer: " << bits_program_pointer
                  << "\nmax_alloc_size: " << max_alloc_size
                  << "\nglb_alloc_aligned_size: " << glb_alloc_aligned_size
                  << "\nloc_alloc_aligned_size: " << loc_alloc_aligned_size
                  << "\nmin_access_size: " << min_access_size
                  << "\nmax_access_size: " << max_access_size
                  << "\nbits_byte: " << bits_byte
                  << "\nbits_poison_per_byte: " << bits_poison_per_byte
                  << "\nstrlen_unroll_cnt: " << strlen_unroll_cnt
                  << "\nmemcmp_unroll_cnt: " << memcmp_unroll_cnt
                  << "\nlittle_endian: " << little_endian
                  << "\nnullptr_is_used: " << has_null_pointer
                  << "\nobserves_addresses: " << observes_addresses
                  << "\nhas_null_block: " << has_null_block
                  << "\ndoes_ptr_store: " << does_ptr_store
                  << "\ndoes_mem_access: " << does_mem_access
                  << "\ndoes_ptr_mem_access: " << does_ptr_mem_access
                  << "\ndoes_int_mem_access: " << does_int_mem_access
                  << '\n';
}


namespace tools {

TransformVerify::TransformVerify(Transform &t, bool check_each_var)
  : t(t), check_each_var(check_each_var) {
  if (check_each_var) {
    for (auto &i : t.tgt.instrs()) {
      tgt_instrs.emplace(i.getName(), &i);
    }
  }
}

pair<unique_ptr<State>, unique_ptr<State>> TransformVerify::exec() const {
  ScopedWatch symexec_watch([](auto &w) {
    if (w.seconds() > 5)
      dbg() << "WARNING: slow vcgen! Took " << w << '\n';
  });

  t.tgt.syncDataWithSrc(t.src);
  calculateAndInitConstants(t);
  State::resetGlobals();

  auto src_state = make_unique<State>(t.src, true);
  auto tgt_state = make_unique<State>(t.tgt, false);
  sym_exec(*src_state);
  tgt_state->syncSEdataWithSrc(*src_state);
  sym_exec(*tgt_state);
  src_state->mkAxioms(*tgt_state);

  return { std::move(src_state), std::move(tgt_state) };
}

Errors TransformVerify::verify() const {
  if (!t.src.getFnAttrs().refinedBy(t.tgt.getFnAttrs()))
    return { "Function attributes not refined", true };

  {
    auto src_inputs = t.src.getInputs();
    auto tgt_inputs = t.tgt.getInputs();
    auto litr = src_inputs.begin(), lend = src_inputs.end();
    auto ritr = tgt_inputs.begin(), rend = tgt_inputs.end();

    while (litr != lend && ritr != rend) {
      auto *lv = dynamic_cast<const Input*>(&*litr);
      auto *rv = dynamic_cast<const Input*>(&*ritr);
      if (lv->getType().toString() != rv->getType().toString())
        return { "Signature mismatch between src and tgt", false };

      if (!lv->getAttributes().refinedBy(rv->getAttributes()))
        return { "Parameter attributes not refined", true };

      ++litr;
      ++ritr;
    }

    if (litr != lend || ritr != rend)
      return { "Signature mismatch between src and tgt", false };
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
  for (auto GVT : globals_tgt) {
    auto I = find_if(globals_src.begin(), globals_src.end(),
      [GVT](auto *GV) -> bool { return GVT->getName() == GV->getName(); });
    if (I != globals_src.end())
      continue;

    if (!GVT->isConst()) {
        string s = "Unsupported interprocedural transformation: non-constant "
                   "global variable " + GVT->getName() + " is introduced in"
                   " target";
        return { std::move(s), false };
    }
  }

  Errors errs;
  try {
    auto [src_state, tgt_state] = exec();

    if (check_each_var) {
      for (auto &[var, val] : src_state->getValues()) {
        auto &name = var->getName();
        if (name[0] != '%' || !dynamic_cast<const Instr*>(var))
          continue;

        auto &val_tgt = tgt_state->at(*tgt_instrs.at(name));
        check_refinement(errs, t, *src_state, *tgt_state, var, var->getType(),
                         val, val_tgt, check_each_var);
        if (errs)
          return errs;
      }
    }

    check_refinement(errs, t, *src_state, *tgt_state, nullptr, t.src.getType(),
                     src_state->returnVal(), tgt_state->returnVal(),
                     check_each_var);
  } catch (AliveException e) {
    return std::move(e);
  }
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
    assert(r.isSat() || r.isUnsat());
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
        c &= i.getType() == tgt_instrs.at(i.getName())->getType();
    }
  }
  return { std::move(c) };
}

void TransformVerify::fixupTypes(const TypingAssignments &ty) {
  if (ty.has_only_one_solution)
    return;
  if (t.precondition)
    t.precondition->fixupTypes(ty.r.getModel());
  t.src.fixupTypes(ty.r.getModel());
  t.tgt.fixupTypes(ty.r.getModel());
}

static map<string_view, Instr*> can_remove_init(Function &fn) {
  map<string_view, Instr*> to_remove;
  auto &bb = fn.getFirstBB();
  if (bb.getName() != "#init")
    return to_remove;

  bool has_int2ptr = false;
  for (auto &i : fn.instrs()) {
    if (isCast(ConversionOp::Int2Ptr, i)) {
      has_int2ptr = true;
      break;
    }
  }

  vector<Value*> worklist;
  set<const Value*> seen;
  auto users = fn.getUsers();

  for (auto &i : bb.instrs()) {
    if (!dynamic_cast<const Store*>(&i))
      continue;
    auto gvar = i.operands()[1];
    worklist.emplace_back(gvar);
    seen.emplace(&i);

    bool needed = false;
    do {
      auto user = worklist.back();
      worklist.pop_back();
      if (!seen.emplace(user).second)
        continue;

      // OK, we can't observe which memory it reads
      if (dynamic_cast<FnCall*>(user))
        continue;

      if (isCast(ConversionOp::Ptr2Int, *user)) {
        // int2ptr can potentially alias with anything, so play on the safe side
        if (has_int2ptr) {
          needed = true;
          break;
        }
        continue;
      }

      // if (p == @const) read(load p) ; this should read @const (or raise UB)
      if (dynamic_cast<ICmp*>(user)) {
        needed = true;
        break;
      }

      // no useful users
      if (dynamic_cast<Return*>(user))
        continue;

      if (dynamic_cast<MemInstr*>(user) && !dynamic_cast<GEP*>(user)) {
        needed = true;
        break;
      }

      if (auto I = users.find(user);
          I != users.end()) {
        for (auto &[user, _] : I->second) {
          worklist.emplace_back(user);
        }
      }
    } while (!worklist.empty());

    worklist.clear();
    seen.clear();

    if (!needed)
      to_remove.emplace(gvar->getName(), const_cast<Instr*>(&i));
  }
  return to_remove;
}

static void remove_unreachable_bbs(Function &f) {
  vector<BasicBlock*> wl = { &f.getFirstBB() };
  set<BasicBlock*> reachable;

  do {
    auto bb = wl.back();
    wl.pop_back();
    if (!reachable.emplace(bb).second || bb->empty())
      continue;

    if (auto instr = dynamic_cast<JumpInstr*>(&bb->back())) {
      for (auto &target : instr->targets()) {
        wl.emplace_back(const_cast<BasicBlock*>(&target));
      }
    }
  } while (!wl.empty());

  auto all_bbs = f.getBBs(); // copy intended
  vector<string> unreachable;
  for (auto bb : all_bbs) {
    if (!reachable.count(bb)) {
      unreachable.emplace_back(bb->getName());
      f.removeBB(*bb);
    }
  }

  for (auto &i : f.instrs()) {
    if (auto phi = dynamic_cast<const Phi*>(&i)) {
      for (auto &bb : unreachable) {
        const_cast<Phi*>(phi)->removeValue(bb);
      }
    }
  }
}

static void optimize_ptrcmp(Function &f) {
  auto is_inbounds = [](const Value &v) {
    if (auto *gep = dynamic_cast<const GEP*>(&v))
      return gep->isInBounds();

    if (!returns_local(v))
      return false;

    if (auto *call = dynamic_cast<const FnCall*>(&v)) {
      auto &attrs = call->getAttributes();
      if (attrs.derefBytes > 0 || attrs.derefOrNullBytes > 0)
        return true;
      return false;
    }
    return true;
  };

  for (auto &i : f.instrs()) {
    auto *icmp = dynamic_cast<const ICmp*>(&i);
    if (!icmp)
      continue;

    auto cond = icmp->getCond();
    bool is_eq = cond == ICmp::EQ || cond == ICmp::NE;
    bool is_signed_cmp = cond == ICmp::SLE || cond == ICmp::SLT ||
                         cond == ICmp::SGE || cond == ICmp::SGT;

    auto ops = icmp->operands();
    auto *op0 = ops[0];
    auto *op1 = ops[1];
    if (is_eq &&
        ((is_inbounds(*op0) && dynamic_cast<const NullPointerValue*>(op1)) ||
         (is_inbounds(*op1) && dynamic_cast<const NullPointerValue*>(op0)))) {
      // (gep inbounds p, ofs) == null
      const_cast<ICmp*>(icmp)->setPtrCmpMode(ICmp::PROVENANCE);
    }

    auto base0 = get_base_ptr(op0);
    auto base1 = get_base_ptr(op1);
    if (base0 && base0 == base1) {
      if (is_eq)
        const_cast<ICmp*>(icmp)->setPtrCmpMode(ICmp::PROVENANCE);
      else if (is_inbounds(*op0) && is_inbounds(*op1) && !is_signed_cmp)
        // Even if op0 and op1 are inbounds, 'icmp slt op0, op1' must
        // compare underlying addresses because it is possible for the block
        // to span across [a, b] where a >s 0 && b <s 0.
        const_cast<ICmp*>(icmp)->setPtrCmpMode(ICmp::OFFSETONLY);
    }
  }
}

void Transform::preprocess() {
  remove_unreachable_bbs(src);
  remove_unreachable_bbs(tgt);

  // remove store of initializers to global variables that aren't needed to
  // verify the transformation
  // We only remove inits if it's possible to remove from both programs to keep
  // memories syntactically equal
  auto remove_init_tgt = can_remove_init(tgt);
  for (auto &[name, isrc] : can_remove_init(src)) {
    auto Itgt = remove_init_tgt.find(name);
    if (Itgt == remove_init_tgt.end())
      continue;
    src.getFirstBB().delInstr(isrc);
    tgt.getFirstBB().delInstr(Itgt->second);
    // TODO: check that tgt init refines that of src
  }

  // remove constants introduced in target
  auto src_gvs = src.getGlobalVarNames();
  for (auto &[name, itgt] : remove_init_tgt) {
    if (find(src_gvs.begin(), src_gvs.end(), name.substr(1)) == src_gvs.end())
      tgt.getFirstBB().delInstr(itgt);
  }

  // optimize pointer comparisons
  optimize_ptrcmp(src);
  optimize_ptrcmp(tgt);

  // remove side-effect free instructions without users
  vector<Instr*> to_remove;
  for (auto fn : { &src, &tgt }) {
    bool changed;
    do {
      auto users = fn->getUsers();
      changed = false;

      for (auto bb : fn->getBBs()) {
        for (auto &i : bb->instrs()) {
          auto i_ptr = const_cast<Instr*>(&i);
          if (hasNoSideEffects(i) && !users.count(i_ptr))
            to_remove.emplace_back(i_ptr);
        }

        for (auto i : to_remove) {
          bb->delInstr(i);
          changed = true;
        }
        to_remove.clear();
      }

      changed |=
        fn->removeUnusedStuff(users, fn == &src ? vector<string_view>()
                                                : src.getGlobalVarNames());
    } while (changed);
  }

  // bits_program_pointer is used by unroll. Initialize it in advance
  initBitsProgramPointer(*this);

  src.unroll(config::src_unroll_cnt);
  tgt.unroll(config::tgt_unroll_cnt);
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
