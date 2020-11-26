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
#include "util/stopwatch.h"
#include "util/symexec.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

using namespace IR;
using namespace smt;
using namespace tools;
using namespace util;
using namespace std;


static bool is_arbitrary(const expr &e) {
  if (e.isConst())
    return false;
  return check_expr(expr::mkForAll(e.vars(), expr::mkVar("#someval", e) != e)).
           isUnsat();
}

static void print_single_varval(ostream &os, State &st, const Model &m,
                                const Value *var, const Type &type,
                                const StateValue &val, unsigned child) {
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
    auto var = in->getUndefVar(type, child);
    if (var.isValid() && m.eval(var, false).isAllOnes()) {
      os << "undef";
      return;
    }
  }

  // TODO: detect undef bits (total or partial) with an SMT query

  expr partial = m.eval(val.value);
  if (is_arbitrary(partial)) {
    os << "any";
    return;
  }

  type.printVal(os, st, m.eval(val.value, true));

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

static void print_varval(ostream &os, State &st, const Model &m,
                         const Value *var, const Type &type,
                         const StateValue &val, unsigned child = 0) {
  if (!type.isAggregateType()) {
    print_single_varval(os, st, m, var, type, val, child);
    return;
  }

  os << (type.isStructType() ? "{ " : "< ");
  auto agg = type.getAsAggregateType();
  for (unsigned i = 0, e = agg->numElementsConst(); i < e; ++i) {
    if (i != 0)
      os << ", ";
    print_varval(os, st, m, var, agg->getChild(i), agg->extract(val, i),
                 child + i);
  }
  os << (type.isStructType() ? " }" : " >");
}


using print_var_val_ty = function<void(ostream&, const Model&)>;

static void error(Errors &errs, State &src_state, State &tgt_state,
                  const Result &r, const Value *var,
                  const char *msg, bool check_each_var,
                  print_var_val_ty print_var_val) {

  if (r.isInvalid()) {
    errs.add("Invalid expr", false);
    auto &unsupported = src_state.getUnsupported();
    if (!unsupported.empty()) {
      string str = "The program uses the following unsupported features: ";
      bool first = true;
      for (auto name : unsupported) {
        if (!first)
          str += ", ";
        str += name;
        first = false;
      }
      errs.add(move(str), false);
    }
    return;
  }

  if (r.isTimeout()) {
    errs.add("Timeout", false);
    return;
  }

  if (r.isError()) {
    errs.add("SMT Error: " + r.getReason(), false);
    return;
  }

  if (r.isSkip()) {
    errs.add("Skip", false);
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

    for (auto &[var, val] : st->getValues()) {
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

    st->getMemory().print(s, m);
  }

  print_var_val(s, m);
  errs.add(s.str(), true);
}


static void instantiate_undef(const Input *in, map<expr, expr> &instances,
                              map<expr, expr> &instances2, const Type &ty,
                              unsigned child) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i < e; ++i) {
      if (!agg->isPadding(i))
        instantiate_undef(in, instances, instances2, agg->getChild(i),
                          child + i);
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

  expr nums[2] = { expr::mkUInt(0, 1), expr::mkUInt(1, 1) };

  for (auto &[e, v] : instances) {
    for (unsigned i = 0; i < 2; ++i) {
      expr newexpr = e.subst(var, nums[i]);
      if (newexpr.eq(e)) {
        instances2[move(newexpr)] = v;
        break;
      }

      newexpr = newexpr.simplify();
      if (newexpr.isFalse())
        continue;

      // keep 'var' variables for counterexample printing
      instances2.try_emplace(move(newexpr), v && var == nums[i]);
    }
  }
  instances = move(instances2);
}

static expr preprocess(Transform &t, const set<expr> &qvars0,
                       const set<expr> &undef_qvars, expr &&e) {
  if (hit_half_memory_limit())
    return expr::mkForAll(qvars0, move(e));

  // eliminate all quantified boolean vars; Z3 gets too slow with those
  auto qvars = qvars0;
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
  }

  if (config::disable_undef_input || undef_qvars.empty() ||
      hit_half_memory_limit())
    return expr::mkForAll(qvars, move(e));

  // manually instantiate undef masks
  map<expr, expr> instances({ { move(e), true } });
  map<expr, expr> instances2;

  for (auto &i : t.src.getInputs()) {
    if (auto in = dynamic_cast<const Input*>(&i))
      instantiate_undef(in, instances, instances2, i.getType(), 0);
  }

  expr insts(false);
  for (auto &[e, v] : instances) {
    insts |= expr::mkForAll(qvars, move(const_cast<expr&>(e))) && v;
  }
  return insts;
}


static expr
encode_undef_refinement_per_elem(const Type &ty, const StateValue &sva,
                                 expr &&a2, expr &&b, expr &&b2) {
  const auto *aty = ty.getAsAggregateType();
  if (!aty)
    return sva.non_poison && sva.value == a2 && b != b2;

  StateValue sva2{move(a2), expr()};
  StateValue svb{move(b), expr()}, svb2{move(b2), expr()};
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
  if (b.second.empty())
    // target is never undef
    return false;

  auto subst = [](const auto &val) {
    vector<pair<expr, expr>> repls;
    for (auto &v : val.second) {
      repls.emplace_back(v, expr::some(v));
    }
    return val.first.value.subst(repls);
  };

  return encode_undef_refinement_per_elem(type, a.first, subst(a),
                                          expr(b.first.value), subst(b));
}

static void
check_refinement(Errors &errs, Transform &t, State &src_state, State &tgt_state,
                 const Value *var, const Type &type,
                 const expr &dom_a, const expr &fndom_a, const State::ValTy &ap,
                 const expr &dom_b, const expr &fndom_b, const State::ValTy &bp,
                 bool check_each_var) {
  auto &a = ap.first;
  auto &b = bp.first;

  auto &uvars = ap.second;
  auto qvars = src_state.getQuantVars();
  qvars.insert(ap.second.begin(), ap.second.end());
  auto &fn_qvars = tgt_state.getFnQuantVars();
  qvars.insert(fn_qvars.begin(), fn_qvars.end());

  auto err = [&](const Result &r, print_var_val_ty print, const char *msg) {
    error(errs, src_state, tgt_state, r, var, msg, check_each_var, print);
  };

  auto print_value = [&](ostream &s, const Model &m) {
    s << "Source value: ";
    print_varval(s, src_state, m, var, type, a);
    s << "\nTarget value: ";
    print_varval(s, tgt_state, m, var, type, b);
  };

  AndExpr axioms = src_state.getAxioms();
  axioms.add(tgt_state.getAxioms());

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

  expr axioms_expr = axioms();
  expr dom = dom_a && dom_b;

  auto sink_tgt = tgt_state.sinkDomain();
  pre_tgt &= !sink_tgt;

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

  auto [poison_cnstr, value_cnstr] = type.refines(src_state, tgt_state, a, b);
  expr undef_cnstr = encode_undef_refinement(type, ap, bp);

  auto src_mem = src_state.returnMemory();
  auto tgt_mem = tgt_state.returnMemory();
  auto [memory_cnstr0, ptr_refinement0, mem_undef]
    = src_mem.refined(tgt_mem, false);
  auto &ptr_refinement = ptr_refinement0;
  auto memory_cnstr = memory_cnstr0.isTrue() ? memory_cnstr0
                                             : value_cnstr && memory_cnstr0;
  qvars.insert(mem_undef.begin(), mem_undef.end());

  if (src_state.sinkDomain().isTrue()) {
    errs.add("The source program doesn't reach a return instruction.\n"
             "Consider increasing the unroll factor if it has loops", false);
    return;
  }

  if (sink_tgt.isTrue()) {
    errs.add("The target program doesn't reach a return instruction.\n"
             "Consider increasing the unroll factor if it has loops", false);
    return;
  }

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
            preprocess(t, qvars, uvars, pre && pre_src_forall.implies(refines));
  };

  auto print_ptr_load = [&](ostream &s, const Model &m) {
    set<expr> undef;
    Pointer p(src_mem, m[ptr_refinement()]);
    unsigned align = bits_byte / 8;
    s << "\nMismatch in " << p
      << "\nSource value: " << Byte(src_mem, m[src_mem.load(p, undef, align)()])
      << "\nTarget value: " << Byte(tgt_mem, m[tgt_mem.load(p, undef, align)()]);
  };

  expr dom_constr;
  if (dom_a.eq(fndom_a)) {
    if (dom_b.eq(fndom_b)) // A /\ B /\ A != B
       dom_constr = false;
    else // A /\ B /\ A != C
      dom_constr = fndom_a && fndom_b && !dom_b;
  } else if (dom_b.eq(fndom_b)) { // A /\ B /\ C != B
    dom_constr = fndom_a && fndom_b && !dom_a;
  } else {
    dom_constr = (fndom_a && fndom_b) && dom_a != dom_b;
  }

  Solver::check({
    { mk_fml(fndom_a.notImplies(fndom_b)),
      [&](const Result &r) {
        err(r, [](ostream&, const Model&){},
            "Source is more defined than target");
      }},
    { mk_fml(move(dom_constr)),
      [&](const Result &r) {
        err(r, [](ostream&, const Model&){},
            "Source and target don't have the same return domain");
      }},
    { mk_fml(dom && !poison_cnstr),
      [&](const Result &r) {
        err(r, print_value, "Target is more poisonous than source");
      }},
    { mk_fml(dom && undef_cnstr),
      [&](const Result &r) {
        err(r, print_value, "Target's return value is more undefined");
      }},
    { mk_fml(dom && !value_cnstr),
      [&](const Result &r) {
        err(r, print_value, "Value mismatch");
      }},
    { mk_fml(dom && !memory_cnstr),
      [&](const Result &r) {
        err(r, print_ptr_load, "Mismatch in memory");
      }}
  });
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
  return dynamic_cast<const Alloc*>(&v) ||
         dynamic_cast<const Malloc*>(&v) ||
         dynamic_cast<const Calloc*>(&v);
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
      for (auto &op : phi->operands())
        todo.emplace_back(op);
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
      auto [ptr, offset] = collect_gep_offsets(load->getPtr());
      rets_nonloc = cache.emplace(ptr, offset).second;
    }
  }
  return rets_nonloc ? num_ptrs(inst.getType()) : 0;
}


static void initBitsProgramPointer(Transform &t) {
  // FIXME: varies among address spaces
  bits_program_pointer = t.src.bitsPointers();
  assert(bits_program_pointer > 0 && bits_program_pointer <= 64);
  assert(bits_program_pointer == t.tgt.bitsPointers());
}

static void calculateAndInitConstants(Transform &t) {
  if (!bits_program_pointer)
    initBitsProgramPointer(t);

  const auto &globals_tgt = t.tgt.getGlobalVars();
  const auto &globals_src = t.src.getGlobalVars();
  num_globals_src = globals_src.size();
  unsigned num_globals = num_globals_src;

  heap_block_alignment = 8;

  num_consts_src = 0;
  num_extra_nonconst_tgt = 0;

  for (auto GV : globals_src) {
    if (GV->isConst())
      ++num_consts_src;
  }

  for (auto GVT : globals_tgt) {
    auto I = find_if(globals_src.begin(), globals_src.end(),
      [GVT](auto *GV) -> bool { return GVT->getName() == GV->getName(); });
    if (I == globals_src.end()) {
      ++num_globals;
      if (!GVT->isConst())
        ++num_extra_nonconst_tgt;
    }
  }

  num_ptrinputs = 0;
  for (auto &arg : t.src.getInputs()) {
    auto n = num_ptrs(arg.getType());
    if (dynamic_cast<const Input*>(&arg)->hasAttribute(ParamAttrs::ByVal)) {
      num_globals_src += n;
      num_globals += n;
    } else
      num_ptrinputs += n;
  }

  // The number of instructions that can return a pointer to a non-local block.
  unsigned num_nonlocals_inst_src = 0, num_nonlocals_inst_tgt = 0;
  // The number of local blocks.
  num_locals_src = 0;
  num_locals_tgt = 0;
  uint64_t max_gep_src = 0, max_gep_tgt = 0;
  uint64_t max_alloc_size = 0;
  uint64_t max_access_size = 0;
  uint64_t min_global_size = UINT64_MAX;

  bool nullptr_is_used = false;
  has_int2ptr      = false;
  has_ptr2int      = false;
  has_alloca       = false;
  has_dead_allocas = false;
  has_malloc       = false;
  has_free         = false;
  has_fncall       = false;
  has_null_block   = false;
  does_ptr_store   = false;
  does_ptr_mem_access = false;
  does_int_mem_access = false;
  bool does_any_byte_access = false;

  // Mininum access size (in bytes)
  uint64_t min_access_size = 8;
  unsigned min_vect_elem_sz = 0;
  bool does_mem_access = false;
  bool has_ptr_load = false;
  bool has_vector_bitcast = false;

  for (auto fn : { &t.src, &t.tgt }) {
    unsigned &cur_num_locals = fn == &t.src ? num_locals_src : num_locals_tgt;
    uint64_t &cur_max_gep    = fn == &t.src ? max_gep_src : max_gep_tgt;
    auto &num_nonlocals_inst = fn == &t.src ? num_nonlocals_inst_src
                                            : num_nonlocals_inst_tgt;

    for (auto &v : fn->getInputs()) {
      auto *i = dynamic_cast<const Input *>(&v);
      if (!i)
        continue;

      uint64_t align = i->hasAttribute(ParamAttrs::Align) ?
          i->getAttributes().getAlign() : 1;
      if (i->hasAttribute(ParamAttrs::Dereferenceable)) {
        does_mem_access = true;
        uint64_t deref_bytes = i->getAttributes().derefBytes;
        min_access_size = gcd(min_access_size, gcd(deref_bytes, align));
        max_access_size = max(max_access_size, deref_bytes);
      }
      if (i->hasAttribute(ParamAttrs::ByVal)) {
        does_mem_access = true;
        auto sz = i->getAttributes().blockSize;
        max_access_size = max(max_access_size, sz);
        min_global_size = min_global_size != UINT64_MAX
                            ? gcd(sz, min_global_size)
                            : sz;
        min_global_size = gcd(min_global_size, align);
      }
    }

    auto update_min_vect_sz = [&](const Type &ty) {
      auto elemsz = minVectorElemSize(ty);
      if (min_vect_elem_sz && elemsz)
        min_vect_elem_sz = gcd(min_vect_elem_sz, elemsz);
      else if (elemsz)
        min_vect_elem_sz = elemsz;
    };

    set<pair<Value*, uint64_t>> nonlocal_cache;
    for (auto BB : fn->getBBs()) {
      for (auto &i : BB->instrs()) {
        if (returns_local(i))
          ++cur_num_locals;
        else
          num_nonlocals_inst += returns_nonlocal(i, nonlocal_cache);

        for (auto op : i.operands()) {
          nullptr_is_used |= has_nullptr(op);
          update_min_vect_sz(op->getType());
        }

        update_min_vect_sz(i.getType());

        if (dynamic_cast<const FnCall*>(&i))
          has_fncall |= true;

        if (auto *mi = dynamic_cast<const MemInstr *>(&i)) {
          max_alloc_size  = max(max_alloc_size, mi->getMaxAllocSize());
          max_access_size = max(max_access_size, mi->getMaxAccessSize());
          cur_max_gep     = add_saturate(cur_max_gep, mi->getMaxGEPOffset());
          has_free       |= mi->canFree();

          auto info = mi->getByteAccessInfo();
          has_ptr_load         |= info.doesPtrLoad;
          does_ptr_store       |= info.doesPtrStore;
          does_int_mem_access  |= info.hasIntByteAccess;
          does_mem_access      |= info.doesMemAccess();
          min_access_size       = gcd(min_access_size, info.byteSize);
          if (info.doesMemAccess() && !info.hasIntByteAccess &&
              !info.doesPtrLoad && !info.doesPtrStore)
            does_any_byte_access = true;

          if (auto alloc = dynamic_cast<const Alloc*>(&i)) {
            has_alloca = true;
            has_dead_allocas |= alloc->initDead();
          } else {
            has_malloc |= dynamic_cast<const Malloc*>(&i) != nullptr ||
                          dynamic_cast<const Calloc*>(&i) != nullptr;
          }

        } else if (isCast(ConversionOp::Int2Ptr, i) ||
                   isCast(ConversionOp::Ptr2Int, i)) {
          max_alloc_size = max_access_size = cur_max_gep = UINT64_MAX;
          has_int2ptr |= isCast(ConversionOp::Int2Ptr, i) != nullptr;
          has_ptr2int |= isCast(ConversionOp::Ptr2Int, i) != nullptr;

        } else if (auto *bc = isCast(ConversionOp::BitCast, i)) {
          auto &t = bc->getType();
          has_vector_bitcast |= t.isVectorType();
          min_access_size = gcd(min_access_size, getCommonAccessSize(t));
        }
      }
    }
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
      min_global_size = gcd(min_global_size, glb->getAlignment());
    }
  }

  // check if null block is needed
  // Global variables cannot be null pointers
  has_null_block = num_ptrinputs > 0 || nullptr_is_used || has_malloc ||
                  has_ptr_load || has_fncall;

  num_nonlocals_src = num_globals_src + num_ptrinputs + num_nonlocals_inst_src +
                      has_null_block;

  // Allow at least one non-const global for calls to change
  num_nonlocals_src += has_fncall;

  num_nonlocals = num_nonlocals_src + num_globals - num_globals_src;

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
  has_readonly = has_attr(ParamAttrs::ReadOnly);
  has_readnone = has_attr(ParamAttrs::ReadNone);
  bits_for_ptrattrs = has_nocapture + has_readonly + has_readnone;

  // ceil(log2(maxblks)) + 1 for local bit
  bits_for_bid = max(1u, ilog2_ceil(max(num_locals, num_nonlocals), false))
                   + (num_locals && num_nonlocals);

  // reserve a multiple of 4 for the number of offset bits to make SMT &
  // counterexamples more readable
  // Allow an extra bit for the sign
  auto max_geps
    = ilog2_ceil(add_saturate(max(max_gep_src, max_gep_tgt), max_access_size),
                 true) + 1;
  bits_for_offset = min(round_up(max_geps, 4), (uint64_t)t.src.bitsPtrOffset());

  // we need an extra bit because 1st bit of size is always 0
  bits_size_t = ilog2_ceil(max_alloc_size, true);
  bits_size_t = min(max(bits_for_offset, bits_size_t)+1, bits_program_pointer);

  // size of byte
  if (num_globals != 0) {
    if (does_mem_access || has_vector_bitcast)
      min_access_size = gcd(min_global_size, min_access_size);
    else {
      min_access_size = min_global_size;
      while (min_access_size > 8) {
        if (min_access_size % 2) {
          min_access_size = 1;
          break;
        }
        min_access_size /= 2;
      }
    }
  }
  bits_byte = 8 * ((does_mem_access || num_globals != 0)
                     ? (unsigned)min_access_size : 1);

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
                  << "\nbits_for_bid: " << bits_for_bid
                  << "\nbits_for_offset: " << bits_for_offset
                  << "\nbits_size_t: " << bits_size_t
                  << "\nbits_program_pointer: " << bits_program_pointer
                  << "\nmax_alloc_size: " << max_alloc_size
                  << "\nmin_access_size: " << min_access_size
                  << "\nmax_access_size: " << max_access_size
                  << "\nbits_byte: " << bits_byte
                  << "\nbits_poison_per_byte: " << bits_poison_per_byte
                  << "\nstrlen_unroll_cnt: " << strlen_unroll_cnt
                  << "\nmemcmp_unroll_cnt: " << memcmp_unroll_cnt
                  << "\nlittle_endian: " << little_endian
                  << "\nnullptr_is_used: " << nullptr_is_used
                  << "\nhas_int2ptr: " << has_int2ptr
                  << "\nhas_ptr2int: " << has_ptr2int
                  << "\nhas_malloc: " << has_malloc
                  << "\nhas_free: " << has_free
                  << "\nhas_null_block: " << has_null_block
                  << "\ndoes_ptr_store: " << does_ptr_store
                  << "\ndoes_mem_access: " << does_mem_access
                  << "\ndoes_ptr_mem_access: " << does_ptr_mem_access
                  << "\ndoes_int_mem_access: " << does_int_mem_access
                  << '\n';
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

pair<unique_ptr<State>, unique_ptr<State>> TransformVerify::exec() const {
  StopWatch symexec_watch;
  t.tgt.syncDataWithSrc(t.src);
  calculateAndInitConstants(t);
  State::resetGlobals();

  auto src_state = make_unique<State>(t.src, true);
  auto tgt_state = make_unique<State>(t.tgt, false);
  sym_exec(*src_state);
  tgt_state->syncSEdataWithSrc(*src_state);
  sym_exec(*tgt_state);
  src_state->mkAxioms(*tgt_state);

  symexec_watch.stop();
  if (symexec_watch.seconds() > 5) {
    cerr << "WARNING: slow vcgen! Took " << symexec_watch << '\n';
  }
  return { move(src_state), move(tgt_state) };
}

Errors TransformVerify::verify() const {
  if (t.src.getFnAttrs() != t.tgt.getFnAttrs() ||
      !t.src.hasSameInputs(t.tgt)) {
    return { "Unsupported interprocedural transformation: signature mismatch "
             "between src and tgt", false };
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

  Errors errs;
  try {
    auto [src_state, tgt_state] = exec();

    if (check_each_var) {
      for (auto &[var, val] : src_state->getValues()) {
        auto &name = var->getName();
        if (name[0] != '%' || !dynamic_cast<const Instr*>(var))
          continue;

        // TODO: add data-flow domain tracking for Alive, but not for TV
        check_refinement(errs, t, *src_state, *tgt_state, var, var->getType(),
                         true, true, val,
                         true, true, tgt_state->at(*tgt_instrs.at(name)),
                         check_each_var);
        if (errs)
          return errs;
      }
    }

    check_refinement(errs, t, *src_state, *tgt_state, nullptr, t.src.getType(),
                     src_state->returnDomain()(), src_state->functionDomain()(),
                     src_state->returnVal(),
                     tgt_state->returnDomain()(), tgt_state->functionDomain()(),
                     tgt_state->returnVal(),
                     check_each_var);
  } catch (AliveException e) {
    return move(e);
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
