// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "ir/function.h"
#include "ir/globals.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/exprs.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <functional>
#include <numeric>
#include <sstream>

using namespace smt;
using namespace util;
using namespace std;

#define RAUW(val)    \
  if (val == &what)  \
    val = &with
#define DEFINE_AS_RETZERO(cls, method) \
  uint64_t cls::method() const { return 0; }
#define DEFINE_AS_RETZEROALIGN(cls, method) \
  pair<uint64_t, unsigned> cls::method() const { return { 0, 1 }; }
#define DEFINE_AS_RETFALSE(cls, method) \
  bool cls::method() const { return false; }
#define DEFINE_AS_EMPTYACCESS(cls) \
  MemInstr::ByteAccessInfo cls::getByteAccessInfo() const \
  { return {}; }

// log2 of max number of var args per function
#define VARARG_BITS 8

namespace {
struct print_type {
  IR::Type &ty;
  const char *pre, *post;

  print_type(IR::Type &ty, const char *pre = "", const char *post = " ")
    : ty(ty), pre(pre), post(post) {}

  friend ostream& operator<<(ostream &os, const print_type &s) {
    auto str = s.ty.toString();
    return str.empty() ? os : (os << s.pre << str << s.post);
  }
};

struct LoopLikeFunctionApproximator {
  // input: (i, is the last iter?)
  // output: (value, nonpoison, UB, continue?)
  using fn_t = function<tuple<expr, expr, AndExpr, expr>(unsigned, bool)>;
  fn_t ith_exec;

  LoopLikeFunctionApproximator(fn_t ith_exec) : ith_exec(move(ith_exec)) {}

  // (value, nonpoison, UB)
  tuple<expr, expr, expr> encode(IR::State &s, unsigned unroll_cnt) {
    AndExpr prefix;
    return _loop(s, prefix, 0, unroll_cnt);
  }

  // (value, nonpoison, UB)
  tuple<expr, expr, expr> _loop(IR::State &s, AndExpr &prefix, unsigned i,
                                unsigned unroll_cnt) {
    bool is_last = i == unroll_cnt - 1;
    auto [res_i, np_i, ub_i, continue_i] = ith_exec(i, is_last);
    auto ub = ub_i();
    prefix.add(ub_i);

    if (is_last)
      s.addPre(prefix().implies(!continue_i));

    if (is_last || continue_i.isFalse() || ub.isFalse())
      return { move(res_i), move(np_i), move(ub) };

    prefix.add(continue_i);
    auto [val_next, np_next, ub_next] = _loop(s, prefix, i + 1, unroll_cnt);
    return { expr::mkIf(continue_i, move(val_next), move(res_i)),
             np_i && continue_i.implies(np_next),
             ub && continue_i.implies(ub_next) };
  }
};

uint64_t getGlobalVarSize(const IR::Value *V) {
  if (auto *V2 = isNoOp(*V))
    return getGlobalVarSize(V2);
  if (auto glb = dynamic_cast<const IR::GlobalVariable *>(V))
    return glb->size();
  return UINT64_MAX;
}

}


namespace IR {

bool Instr::propagatesPoison() const {
  // be on the safe side
  return false;
}

expr Instr::getTypeConstraints() const {
  UNREACHABLE();
  return {};
}


BinOp::BinOp(Type &type, string &&name, Value &lhs, Value &rhs, Op op,
             unsigned flags)
  : Instr(type, move(name)), lhs(&lhs), rhs(&rhs), op(op), flags(flags) {
  switch (op) {
  case Add:
  case Sub:
  case Mul:
  case Shl:
    assert((flags & (NSW | NUW)) == flags);
    break;
  case SDiv:
  case UDiv:
  case AShr:
  case LShr:
    assert((flags & Exact) == flags);
    break;
  default:
    assert(flags == None);
    break;
  }
}

vector<Value*> BinOp::operands() const {
  return { lhs, rhs };
}

bool BinOp::propagatesPoison() const {
  return true;
}

void BinOp::rauw(const Value &what, Value &with) {
  RAUW(lhs);
  RAUW(rhs);
}

void BinOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case Add:           str = "add "; break;
  case Sub:           str = "sub "; break;
  case Mul:           str = "mul "; break;
  case SDiv:          str = "sdiv "; break;
  case UDiv:          str = "udiv "; break;
  case SRem:          str = "srem "; break;
  case URem:          str = "urem "; break;
  case Shl:           str = "shl "; break;
  case AShr:          str = "ashr "; break;
  case LShr:          str = "lshr "; break;
  case SAdd_Sat:      str = "sadd_sat "; break;
  case UAdd_Sat:      str = "uadd_sat "; break;
  case SSub_Sat:      str = "ssub_sat "; break;
  case USub_Sat:      str = "usub_sat "; break;
  case SShl_Sat:      str = "sshl_sat "; break;
  case UShl_Sat:      str = "ushl_sat "; break;
  case And:           str = "and "; break;
  case Or:            str = "or "; break;
  case Xor:           str = "xor "; break;
  case Cttz:          str = "cttz "; break;
  case Ctlz:          str = "ctlz "; break;
  case SAdd_Overflow: str = "sadd_overflow "; break;
  case UAdd_Overflow: str = "uadd_overflow "; break;
  case SSub_Overflow: str = "ssub_overflow "; break;
  case USub_Overflow: str = "usub_overflow "; break;
  case SMul_Overflow: str = "smul_overflow "; break;
  case UMul_Overflow: str = "umul_overflow "; break;
  case UMin:          str = "umin "; break;
  case UMax:          str = "umax "; break;
  case SMin:          str = "smin "; break;
  case SMax:          str = "smax "; break;
  case Abs:           str = "abs "; break;
  }

  os << getName() << " = " << str;

  if (flags & NSW)
    os << "nsw ";
  if (flags & NUW)
    os << "nuw ";
  if (flags & Exact)
    os << "exact ";
  os << *lhs << ", " << rhs->getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  // addUB(bp) is not needed because it is registered by getAndAddPoisonUB.
  s.addUB(b != 0);
  if (sign)
    s.addUB((ap && a != expr::IntSMin(b.bits())) || b != expr::mkInt(-1, b));
}

StateValue BinOp::toSMT(State &s) const {
  bool vertical_zip = false;
  function<StateValue(const expr&, const expr&, const expr&, const expr&)>
    fn, scalar_op;

  switch (op) {
  case Add:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.add_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.add_no_uoverflow(b);
      return { a + b, move(non_poison) };
    };
    break;

  case Sub:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.sub_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.sub_no_uoverflow(b);
      return { a - b, move(non_poison) };
    };
    break;

  case Mul:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.mul_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.mul_no_uoverflow(b);
      return { a * b, move(non_poison) };
    };
    break;

  case SDiv:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      div_ub(s, a, b, ap, bp, true);
      if (flags & Exact)
        non_poison = a.sdiv_exact(b);
      return { a.sdiv(b), move(non_poison) };
    };
    break;

  case UDiv:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      div_ub(s, a, b, ap, bp, false);
      if (flags & Exact)
        non_poison &= a.udiv_exact(b);
      return { a.udiv(b), move(non_poison) };
    };
    break;

  case SRem:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      div_ub(s, a, b, ap, bp, true);
      return { a.srem(b), true };
    };
    break;

  case URem:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      div_ub(s, a, b, ap, bp, false);
      return { a.urem(b), true };
    };
    break;

  case Shl:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & NSW)
        non_poison &= a.shl_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.shl_no_uoverflow(b);

      return { a << b, move(non_poison) };
    };
    break;

  case AShr:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & Exact)
        non_poison &= a.ashr_exact(b);
      return { a.ashr(b), move(non_poison) };
    };
    break;

  case LShr:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & Exact)
        non_poison &= a.lshr_exact(b);
      return { a.lshr(b), move(non_poison) };
    };
    break;

  case SAdd_Sat:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.sadd_sat(b), true };
    };
    break;

  case UAdd_Sat:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.uadd_sat(b), true };
    };
    break;

  case SSub_Sat:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.ssub_sat(b), true };
    };
    break;

  case USub_Sat:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.usub_sat(b), true };
    };
    break;

  case SShl_Sat:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return {a.sshl_sat(b), b.ult(b.bits())};
    };
    break;

  case UShl_Sat:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return {a.ushl_sat(b), b.ult(b.bits())};
    };
    break;

  case And:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a & b, true };
    };
    break;

  case Or:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a | b, true };
    };
    break;

  case Xor:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a ^ b, true };
    };
    break;

  case Cttz:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.cttz(expr::mkUInt(a.bits(), a)),
               b == 0u || a != 0u };
    };
    break;

  case Ctlz:
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.ctlz(),
               b == 0u || a != 0u };
    };
    break;

  case SAdd_Overflow:
    vertical_zip = true;
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a + b, (!a.add_no_soverflow(b)).toBVBool() };
    };
    break;

  case UAdd_Overflow:
    vertical_zip = true;
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a + b, (!a.add_no_uoverflow(b)).toBVBool() };
    };
    break;

  case SSub_Overflow:
    vertical_zip = true;
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a - b, (!a.sub_no_soverflow(b)).toBVBool() };
    };
    break;

  case USub_Overflow:
    vertical_zip = true;
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a - b, (!a.sub_no_uoverflow(b)).toBVBool() };
    };
    break;

  case SMul_Overflow:
    vertical_zip = true;
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a * b, (!a.mul_no_soverflow(b)).toBVBool() };
    };
    break;

  case UMul_Overflow:
    vertical_zip = true;
    fn = [](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a * b, (!a.mul_no_uoverflow(b)).toBVBool() };
    };
    break;

  case UMin:
  case UMax:
  case SMin:
  case SMax:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr v;
      switch (op) {
      case UMin:
        v = a.umin(b);
        break;
      case UMax:
        v = a.umax(b);
        break;
      case SMin:
        v = a.smin(b);
        break;
      case SMax:
        v = a.smax(b);
        break;
      default:
        UNREACHABLE();
      }
      return { move(v), ap && bp };
    };
    break;

  case Abs:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.abs(), ap && bp && (b == 0 || a != expr::IntSMin(a.bits())) };
    };
    break;
  }

  function<pair<StateValue,StateValue>(const expr&, const expr&, const expr&,
                                       const expr&)> zip_op;
  if (vertical_zip) {
    zip_op = [&](auto &a, auto &ap, auto &b, auto &bp) {
      auto [v1, v2] = fn(a, ap, b, bp);
      expr non_poison = ap && bp;
      StateValue sv1(move(v1), expr(non_poison));
      return make_pair(move(sv1), StateValue(move(v2), move(non_poison)));
    };
  } else {
    scalar_op = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto [v, np] = fn(a, ap, b, bp);
      return { move(v), ap && (isDivOrRem() ? expr(true) : bp) && np };
    };
  }

  auto &a = s[*lhs];
  auto &b = isDivOrRem() ? s.getAndAddPoisonUB(*rhs) : s[*rhs];

  if (lhs->getType().isVectorType()) {
    auto retty = getType().getAsAggregateType();
    vector<StateValue> vals;

    if (vertical_zip) {
      auto ty = lhs->getType().getAsAggregateType();
      vector<StateValue> vals1, vals2;
      unsigned val2idx = 1 + retty->isPadding(1);
      auto val1ty = retty->getChild(0).getAsAggregateType();
      auto val2ty = retty->getChild(val2idx).getAsAggregateType();

      for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
        auto ai = ty->extract(a, i);
        auto bi = ty->extract(b, i);
        auto [v1, v2] = zip_op(ai.value, ai.non_poison, bi.value,
                               bi.non_poison);
        vals1.emplace_back(move(v1));
        vals2.emplace_back(move(v2));
      }
      vals.emplace_back(val1ty->aggregateVals(vals1));
      vals.emplace_back(val2ty->aggregateVals(vals2));
    } else {
      StateValue tmp;
      for (unsigned i = 0, e = retty->numElementsConst(); i != e; ++i) {
        auto ai = retty->extract(a, i);
        const StateValue *bi;
        switch (op) {
        case Abs:
        case Cttz:
        case Ctlz:
          bi = &b;
          break;
        default:
          tmp = retty->extract(b, i);
          bi = &tmp;
          break;
        }
        vals.emplace_back(scalar_op(ai.value, ai.non_poison, bi->value,
                                    bi->non_poison));
      }
    }
    return retty->aggregateVals(vals);
  }

  if (vertical_zip) {
    vector<StateValue> vals;
    auto [v1, v2] = zip_op(a.value, a.non_poison, b.value, b.non_poison);
    vals.emplace_back(move(v1));
    vals.emplace_back(move(v2));
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return scalar_op(a.value, a.non_poison, b.value, b.non_poison);
}

expr BinOp::getTypeConstraints(const Function &f) const {
  expr instrconstr;
  switch (op) {
  case SAdd_Overflow:
  case UAdd_Overflow:
  case SSub_Overflow:
  case USub_Overflow:
  case SMul_Overflow:
  case UMul_Overflow:
    instrconstr = getType().enforceStructType() &&
                  lhs->getType().enforceIntOrVectorType() &&
                  lhs->getType() == rhs->getType();

    if (auto ty = getType().getAsStructType()) {
      unsigned v2idx = 1 + ty->isPadding(1);
      instrconstr &= ty->numElementsExcludingPadding() == 2 &&
                     ty->getChild(0) == lhs->getType() &&
                     ty->getChild(v2idx).enforceIntOrVectorType(1) &&
                     ty->getChild(v2idx).enforceVectorTypeEquiv(lhs->getType());
    }
    break;
  case Cttz:
  case Ctlz:
  case Abs:
    instrconstr = getType().enforceIntOrVectorType() &&
                  getType() == lhs->getType() &&
                  rhs->getType().enforceIntType(1);
    break;
  default:
    instrconstr = getType().enforceIntOrVectorType() &&
                  getType() == lhs->getType() &&
                  getType() == rhs->getType();
    break;
  }
  return Value::getTypeConstraints() && move(instrconstr);
}

unique_ptr<Instr> BinOp::dup(const string &suffix) const {
  return make_unique<BinOp>(getType(), getName()+suffix, *lhs, *rhs, op, flags);
}

bool BinOp::isDivOrRem() const {
  switch (op) {
  case Op::SDiv:
  case Op::SRem:
  case Op::UDiv:
  case Op::URem:
    return true;
  default:
    return false;
  }
}


vector<Value*> FpBinOp::operands() const {
  return { lhs, rhs };
}

bool FpBinOp::propagatesPoison() const {
  return true;
}

void FpBinOp::rauw(const Value &what, Value &with) {
  RAUW(lhs);
  RAUW(rhs);
}

void FpBinOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FAdd:     str = "fadd "; break;
  case FSub:     str = "fsub "; break;
  case FMul:     str = "fmul "; break;
  case FDiv:     str = "fdiv "; break;
  case FRem:     str = "frem "; break;
  case FMax:     str = "fmax "; break;
  case FMin:     str = "fmin "; break;
  case FMaximum: str = "fmaximum "; break;
  case FMinimum: str = "fminimum "; break;
  case CopySign: str = "copysign "; break;
  }
  os << getName() << " = " << str << fmath << *lhs << ", " << rhs->getName()
     << ", rounding=" << rm;
}

static expr any_fp_zero(State &s, const expr &v) {
  expr is_zero = v.isFPZero();
  if (is_zero.isFalse())
    return v;

  expr var = expr::mkFreshVar("anyzero", true);
  s.addQuantVar(var);
  return expr::mkIf(var && is_zero, v.fneg(), v);
}

static StateValue fm_poison(State &s, const expr &a, const expr &ap,
                            const expr &b, const expr &bp, const expr &c,
                            const expr &cp,
                            function<expr(expr&,expr&,expr&)> fn,
                            FastMathFlags fmath, bool only_input,
                            int nary = 3) {
  expr new_a, new_b, new_c;
  if (fmath.flags & FastMathFlags::NSZ) {
    new_a = any_fp_zero(s, a);
    if (nary >= 2) {
      new_b = any_fp_zero(s, b);
      if (nary == 3)
        new_c = any_fp_zero(s, c);
    }
  } else {
    new_a = a;
    new_b = b;
    new_c = c;
  }

  expr val = fn(new_a, new_b, new_c);
  AndExpr non_poison;
  non_poison.add(ap);
  if (nary >= 2)
    non_poison.add(bp);
  if (nary >= 3)
    non_poison.add(cp);

  if (fmath.flags & FastMathFlags::NNaN) {
    non_poison.add(!a.isNaN());
    if (nary >= 2) {
      non_poison.add(!b.isNaN());
      if (nary == 3)
        non_poison.add(!c.isNaN());
    }
    if (!only_input)
      non_poison.add(!val.isNaN());
  }
  if (fmath.flags & FastMathFlags::NInf) {
    non_poison.add(!a.isInf());
    if (nary >= 2) {
      non_poison.add(!b.isInf());
      if (nary == 3)
        non_poison.add(!c.isInf());
    }
    if (!only_input)
      non_poison.add(!val.isInf());
  }
  if (fmath.flags & FastMathFlags::ARCP) {
    val = expr::mkUF("arcp", { val }, val);
    s.doesApproximation("arcp", val);
  }
  if (fmath.flags & FastMathFlags::Contract) {
    val = expr::mkUF("contract", { val }, val);
    s.doesApproximation("contract", val);
  }
  if (fmath.flags & FastMathFlags::Reassoc) {
    val = expr::mkUF("reassoc", { val }, val);
    s.doesApproximation("reassoc", val);
  }
  if (fmath.flags & FastMathFlags::AFN) {
    val = expr::mkUF("afn", { val }, val);
    s.doesApproximation("afn", val);
  }
  if (fmath.flags & FastMathFlags::NSZ && !only_input)
    val = any_fp_zero(s, move(val));

  return { move(val), non_poison() };
}

static StateValue fm_poison(State &s, const expr &a, const expr &ap,
                            const expr &b, const expr &bp,
                            function<expr(expr&,expr&)> fn,
                            FastMathFlags fmath, bool only_input) {
  return fm_poison(s, move(a), ap, move(b), bp, expr(), expr(),
                   [&](expr &a, expr &b, expr &c) { return fn(a, b); },
                   fmath, only_input, 2);
}

static StateValue fm_poison(State &s, const expr &a, const expr &ap,
                            function<expr(expr&)> fn,
                            FastMathFlags fmath, bool only_input) {
  return fm_poison(s, move(a), ap, expr(), expr(), expr(), expr(),
                   [&](expr &a, expr &b, expr &c) { return fn(a); },
                   fmath, only_input, 1);
}

static StateValue round_value(const function<StateValue(FpRoundingMode)> &fn,
                              const State &s, FpRoundingMode rm) {
  if (!rm.isDynamic())
    return fn(rm);

  auto &var = s.getFpRoundingMode();
  return StateValue::mkIf(var == FpRoundingMode::RNE, fn(FpRoundingMode::RNE),
         StateValue::mkIf(var == FpRoundingMode::RNA, fn(FpRoundingMode::RNA),
         StateValue::mkIf(var == FpRoundingMode::RTP, fn(FpRoundingMode::RTP),
         StateValue::mkIf(var == FpRoundingMode::RTN, fn(FpRoundingMode::RTN),
                          fn(FpRoundingMode::RTZ)))));
}

static expr get_fp_rounding(const State &s) {
  auto &var = s.getFpRoundingMode();
  return expr::mkIf(var == FpRoundingMode::RNE, expr::rne(),
         expr::mkIf(var == FpRoundingMode::RNA, expr::rna(),
         expr::mkIf(var == FpRoundingMode::RTP, expr::rtp(),
         expr::mkIf(var == FpRoundingMode::RTN, expr::rtn(), expr::rtz()))));
}

StateValue FpBinOp::toSMT(State &s) const {
  function<StateValue(const expr&, const expr&, const expr&, const expr&,
                      FpRoundingMode)> fn;

  switch (op) {
  case FAdd:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      return fm_poison(s, a, ap, b, bp,
                       [&](expr &a, expr &b) { return a.fadd(b, rm.toSMT()); },
                       fmath, false);
    };
    break;

  case FSub:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      return fm_poison(s, a, ap, b, bp,
                       [&](expr &a, expr &b) { return a.fsub(b, rm.toSMT()); },
                       fmath, false);
    };
    break;

  case FMul:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      return fm_poison(s, a, ap, b, bp,
                       [&](expr &a, expr &b) { return a.fmul(b, rm.toSMT()); },
                       fmath, false);
    };
    break;

  case FDiv:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      return fm_poison(s, a, ap, b, bp,
                       [&](expr &a, expr &b) { return a.fdiv(b, rm.toSMT()); },
                       fmath, false);
    };
    break;

  case FRem:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      // TODO; Z3 has no support for LLVM's frem which is actually an fmod
      return fm_poison(s, a, ap, b, bp,
                       [&](expr &a, expr &b) {
                         auto val = expr::mkUF("fmod", {a, b}, a);
                         s.doesApproximation("frem", val);
                         return val;
                       },
                       fmath, false);
    };
    break;

  case FMin:
  case FMax:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      expr ndet = expr::mkFreshVar("maxminnondet", true);
      s.addQuantVar(ndet);
      auto ndz = expr::mkIf(ndet, expr::mkNumber("0", a),
                            expr::mkNumber("-0", a));

      auto v = [&](expr &a, expr &b) {
        expr z = a.isFPZero() && b.isFPZero();
        expr cmp = op == FMin ? a.fole(b) : a.foge(b);
        return expr::mkIf(a.isNaN(), b,
                          expr::mkIf(b.isNaN(), a,
                                     expr::mkIf(z, ndz,
                                                expr::mkIf(cmp, a, b))));
      };
      return fm_poison(s, a, ap, b, bp, v, fmath, false);
    };
    break;

  case FMinimum:
  case FMaximum:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      auto v = [&](expr &a, expr &b) {
        expr zpos = expr::mkNumber("0", a), zneg = expr::mkNumber("-0", a);
        expr cmp = (op == FMinimum) ? a.fole(b) : a.foge(b);
        expr neg_cond = op == FMinimum ? (a.isFPNegative() || b.isFPNegative())
                                       : (a.isFPNegative() && b.isFPNegative());
        expr e = expr::mkIf(a.isFPZero() && b.isFPZero(),
                            expr::mkIf(neg_cond, zneg, zpos),
                            expr::mkIf(cmp, a, b));

        return expr::mkIf(a.isNaN(), a, expr::mkIf(b.isNaN(), b, e));
      };
      return fm_poison(s, a, ap, b, bp, v, fmath, false);
    };
    break;
  case CopySign:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp, auto rm) -> StateValue {
      return fm_poison(s, a, ap, b, bp,
                       [](expr &a, expr &b) {
                         return expr::mkIf(a.isFPNegative() == b.isFPNegative(),
                                           a, a.fneg()); },
                       fmath, false);
    };
    break;
  }

  auto scalar = [&](auto &a, auto &ap, auto &b, auto &bp) {
    return round_value([&](auto rm) { return fn(a, ap, b, bp, rm); }, s, rm);
  };

  auto &a = s[*lhs];
  auto &b = s[*rhs];

  if (lhs->getType().isVectorType()) {
    auto retty = getType().getAsAggregateType();
    vector<StateValue> vals;
    for (unsigned i = 0, e = retty->numElementsConst(); i != e; ++i) {
      auto ai = retty->extract(a, i);
      auto bi = retty->extract(b, i);
      vals.emplace_back(
        scalar(ai.value, ai.non_poison, bi.value, bi.non_poison));
    }
    return retty->aggregateVals(vals);
  }
  return scalar(a.value, a.non_poison, b.value, b.non_poison);
}

expr FpBinOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceFloatOrVectorType() &&
         getType() == lhs->getType() &&
         getType() == rhs->getType();
}

unique_ptr<Instr> FpBinOp::dup(const string &suffix) const {
  return make_unique<FpBinOp>(getType(), getName()+suffix, *lhs, *rhs, op,
                              fmath);
}


vector<Value*> UnaryOp::operands() const {
  return { val };
}

bool UnaryOp::propagatesPoison() const {
  return true;
}

void UnaryOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void UnaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case Copy:        str = ""; break;
  case BitReverse:  str = "bitreverse "; break;
  case BSwap:       str = "bswap "; break;
  case Ctpop:       str = "ctpop "; break;
  case IsConstant:  str = "is.constant "; break;
  case FFS:         str = "ffs "; break;
  }

  os << getName() << " = " << str << *val;
}

StateValue UnaryOp::toSMT(State &s) const {
  function<StateValue(const expr&, const expr&)> fn;

  switch (op) {
  case Copy:
    if (dynamic_cast<AggregateValue *>(val))
      // Aggregate value is not registered at state.
      return val->toSMT(s);
    return s[*val];
  case BitReverse:
    fn = [](auto &v, auto np) -> StateValue {
      return { v.bitreverse(), expr(np) };
    };
    break;
  case BSwap:
    fn = [](auto &v, auto np) -> StateValue {
      return { v.bswap(), expr(np) };
    };
    break;
  case Ctpop:
    fn = [](auto &v, auto np) -> StateValue {
      return { v.ctpop(), expr(np) };
    };
    break;
  case IsConstant: {
    expr one = expr::mkUInt(1, 1);
    if (dynamic_cast<Constant *>(val))
      return { move(one), true };

    // may or may not be a constant
    expr var = expr::mkFreshVar("is.const", one);
    s.addQuantVar(var);
    return { move(var), true };
  }
  case FFS:
    fn = [](auto &v, auto np) -> StateValue {
      return { v.cttz(expr::mkInt(-1, v)) + expr::mkUInt(1, v), expr(np) };
    };
    break;
  }

  auto &v = s[*val];

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = val->getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      auto vi = ty->extract(v, i);
      vals.emplace_back(fn(vi.value, vi.non_poison));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return fn(v.value, v.non_poison);
}

expr UnaryOp::getTypeConstraints(const Function &f) const {
  expr instrconstr = getType() == val->getType();
  switch (op) {
  case Copy:
    break;
  case BSwap:
    instrconstr &= getType().enforceScalarOrVectorType([](auto &scalar) {
                     return scalar.enforceIntType() &&
                            scalar.sizeVar().urem(expr::mkUInt(16, 8)) == 0;
                   });
    break;
  case BitReverse:
  case Ctpop:
  case FFS:
    instrconstr &= getType().enforceIntOrVectorType();
    break;
  case IsConstant:
    instrconstr = getType().enforceIntType(1);
    break;
  }

  return Value::getTypeConstraints() && move(instrconstr);
}

unique_ptr<Instr> UnaryOp::dup(const string &suffix) const {
  return make_unique<UnaryOp>(getType(), getName() + suffix, *val, op);
}


vector<Value*> FpUnaryOp::operands() const {
  return { val };
}

bool FpUnaryOp::propagatesPoison() const {
  return true;
}

void FpUnaryOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void FpUnaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FAbs:      str = "fabs "; break;
  case FNeg:      str = "fneg "; break;
  case Ceil:      str = "ceil "; break;
  case Floor:     str = "floor "; break;
  case RInt:      str = "rint "; break;
  case NearbyInt: str = "nearbyint "; break;
  case Round:     str = "round "; break;
  case RoundEven: str = "roundeven "; break;
  case Trunc:     str = "trunc "; break;
  case Sqrt:      str = "sqrt "; break;
  }

  os << getName() << " = " << str << fmath << *val << ", rounding=" << rm;
}

StateValue FpUnaryOp::toSMT(State &s) const {
  function<StateValue(const expr&, const expr&, FpRoundingMode)> fn;

  switch (op) {
  case FAbs:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v) { return v.fabs(); }, fmath, true);
    };
    break;
  case FNeg:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v){ return v.fneg(); }, fmath, false);
    };
    break;
  case Ceil:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v) { return v.ceil(); }, fmath, true);
    };
    break;
  case Floor:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v) { return v.floor(); }, fmath,
                       true);
    };
    break;
  case RInt:
  case NearbyInt:
    // TODO: they differ in exception behavior
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [&](expr &v) { return v.round(rm.toSMT()); },
                       fmath, true);
    };
    break;
  case Round:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v) { return v.round(expr::rna()); },
                       fmath, true);
    };
    break;
  case RoundEven:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v) { return v.round(expr::rne()); },
                       fmath, true);
    };
    break;
  case Trunc:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [](expr &v) { return v.round(expr::rtz()); },
                       fmath, true);
    };
    break;
  case Sqrt:
    fn = [&](auto &v, auto &np, auto rm) -> StateValue {
      return fm_poison(s, v, np, [&](expr &v){ return v.sqrt(rm.toSMT()); },
                       fmath, false);
    };
    break;
  }

  auto scalar = [&](const StateValue &v) {
    return round_value([&](auto rm) { return fn(v.value, v.non_poison, rm); },
                       s, rm);
  };

  auto &v = s[*val];

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = val->getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(v, i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return scalar(v);
}

expr FpUnaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val->getType() &&
         getType().enforceFloatOrVectorType();
}

unique_ptr<Instr> FpUnaryOp::dup(const string &suffix) const {
  return
    make_unique<FpUnaryOp>(getType(), getName() + suffix, *val, op, fmath, rm);
}


vector<Value*> UnaryReductionOp::operands() const {
  return { val };
}

bool UnaryReductionOp::propagatesPoison() const {
  return true;
}

void UnaryReductionOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void UnaryReductionOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case Add:  str = "reduce_add "; break;
  case Mul:  str = "reduce_mul "; break;
  case And:  str = "reduce_and "; break;
  case Or:   str = "reduce_or "; break;
  case Xor:  str = "reduce_xor "; break;
  case SMax:  str = "reduce_smax "; break;
  case SMin:  str = "reduce_smin "; break;
  case UMax:  str = "reduce_umax "; break;
  case UMin:  str = "reduce_umin "; break;
  }

  os << getName() << " = " << str << print_type(val->getType())
     << val->getName();
}

StateValue UnaryReductionOp::toSMT(State &s) const {
  auto &v = s[*val];
  auto vty = val->getType().getAsAggregateType();
  StateValue res;
  for (unsigned i = 0, e = vty->numElementsConst(); i != e; ++i) {
    auto ith = vty->extract(v, i);
    if (i == 0) {
      res = move(ith);
      continue;
    }
    switch (op) {
    case Add: res.value = res.value + ith.value; break;
    case Mul: res.value = res.value * ith.value; break;
    case And: res.value = res.value & ith.value; break;
    case Or:  res.value = res.value | ith.value; break;
    case Xor: res.value = res.value ^ ith.value; break;
    case SMax: res.value = res.value.smax(ith.value); break;
    case SMin: res.value = res.value.smin(ith.value); break;
    case UMax: res.value = res.value.umax(ith.value); break;
    case UMin: res.value = res.value.umin(ith.value); break;
    default:  UNREACHABLE();
    }
    // The result is non-poisonous if all lanes are non-poisonous.
    res.non_poison &= ith.non_poison;
  }
  return res;
}

expr UnaryReductionOp::getTypeConstraints(const Function &f) const {
  expr instrconstr = getType() == val->getType();
  switch(op) {
  case Add:
  case Mul:
  case And:
  case Or:
  case Xor:
  case SMax:
  case SMin:
  case UMax:
  case UMin:
    instrconstr = getType().enforceIntType();
    instrconstr &= val->getType().enforceVectorType(
        [this](auto &scalar) { return scalar == getType(); });
    break;
  }

  return Value::getTypeConstraints() && move(instrconstr);
}

unique_ptr<Instr> UnaryReductionOp::dup(const string &suffix) const {
  return make_unique<UnaryReductionOp>(getType(), getName() + suffix, *val, op);
}


vector<Value*> TernaryOp::operands() const {
  return { a, b, c };
}

bool TernaryOp::propagatesPoison() const {
  return true;
}

void TernaryOp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
  RAUW(c);
}

void TernaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FShl: str = "fshl "; break;
  case FShr: str = "fshr "; break;
  }

  os << getName() << " = " << str << *a << ", " << *b << ", " << *c;
}

StateValue TernaryOp::toSMT(State &s) const {
  auto &av = s[*a];
  auto &bv = s[*b];
  auto &cv = s[*c];
  function<expr(const expr&, const expr&, const expr&)> fn;

  switch (op) {
  case FShl:
    fn = [](auto &a, auto &b, auto &c) -> expr {
      return expr::fshl(a, b, c);
    };
    break;
  case FShr:
    fn = [](auto &a, auto &b, auto &c) -> expr {
      return expr::fshr(a, b, c);
    };
    break;
  }

  auto scalar = [&](const auto &a, const auto &b, const auto &c) -> StateValue {
    return { fn(a.value, b.value, c.value),
             a.non_poison && b.non_poison && c.non_poison };
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(av, i), ty->extract(bv, i),
                               ty->extract(cv, i)));
    }
    return ty->aggregateVals(vals);
  }
  return scalar(av, bv, cv);
}

expr TernaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == a->getType() &&
         getType() == b->getType() &&
         getType() == c->getType() &&
         getType().enforceIntOrVectorType();
}

unique_ptr<Instr> TernaryOp::dup(const string &suffix) const {
  return make_unique<TernaryOp>(getType(), getName() + suffix, *a, *b, *c, op);
}


vector<Value*> FpTernaryOp::operands() const {
  return { a, b, c };
}

bool FpTernaryOp::propagatesPoison() const {
  return true;
}

void FpTernaryOp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
  RAUW(c);
}

void FpTernaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FMA:    str = "fma "; break;
  case MulAdd: str = "fmuladd "; break;
  }

  os << getName() << " = " << str << fmath << *a << ", " << *b << ", " << *c
     << ", rounding=" << rm;
}

StateValue FpTernaryOp::toSMT(State &s) const {
  auto &av = s[*a];
  auto &bv = s[*b];
  auto &cv = s[*c];
  function<StateValue(const StateValue&, const StateValue&, const StateValue&,
                      FpRoundingMode)> fn;

  switch (op) {
  case FMA:
    fn = [&](auto &a, auto &b, auto &c, auto rm) -> StateValue {
      return fm_poison(s, a.value, a.non_poison, b.value, b.non_poison, c.value,
                       c.non_poison, [&](expr &a, expr &b, expr &c) {
                         return expr::fma(a, b, c, rm.toSMT());
                       }, fmath, false);
    };
    break;
  case MulAdd:
    fn = [&](auto &a, auto &b, auto &c, auto rm0) -> StateValue {
      auto rm = rm0.toSMT();
      expr var = expr::mkFreshVar("nondet", expr(false));
      s.addQuantVar(var);
      return fm_poison(s, a.value, a.non_poison, b.value, b.non_poison, c.value,
                       c.non_poison, [&](expr &a, expr &b, expr &c) {
                         return expr::mkIf(var, expr::fma(a, b, c, rm),
                                           a.fmul(b, rm).fadd(c, rm));
                       }, fmath, false);
    };
    break;
  }

  auto scalar = [&](const StateValue &a, const StateValue &b,
                    const StateValue &c) {
    return round_value([&](auto rm) { return fn(a, b, c, rm); }, s, rm);
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(av, i), ty->extract(bv, i),
                               ty->extract(cv, i)));
    }
    return ty->aggregateVals(vals);
  }
  return scalar(av, bv, cv);
}

expr FpTernaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == a->getType() &&
         getType() == b->getType() &&
         getType() == c->getType() &&
         getType().enforceFloatOrVectorType();
}

unique_ptr<Instr> FpTernaryOp::dup(const string &suffix) const {
  return make_unique<FpTernaryOp>(getType(), getName() + suffix, *a, *b, *c, op,
                                  fmath, rm);
}


vector<Value*> ConversionOp::operands() const {
  return { val };
}

bool ConversionOp::propagatesPoison() const {
  return true;
}

void ConversionOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void ConversionOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case SExt:     str = "sext "; break;
  case ZExt:     str = "zext "; break;
  case Trunc:    str = "trunc "; break;
  case BitCast:  str = "bitcast "; break;
  case Ptr2Int:  str = "ptrtoint "; break;
  case Int2Ptr:  str = "int2ptr "; break;
  }

  os << getName() << " = " << str << *val << print_type(getType(), " to ", "");
}

StateValue ConversionOp::toSMT(State &s) const {
  auto v = s[*val];
  function<expr(expr &&, const Type &)> fn;

  switch (op) {
  case SExt:
    fn = [](auto &&val, auto &to_type) -> expr {
      return val.sext(to_type.bits() - val.bits());
    };
    break;
  case ZExt:
    fn = [](auto &&val, auto &to_type) -> expr {
      return val.zext(to_type.bits() - val.bits());
    };
    break;
  case Trunc:
    fn = [](auto &&val, auto &to_type) -> expr {
      return val.trunc(to_type.bits());
    };
    break;
  case BitCast:
    fn = [](auto &&val, auto &to_type) -> expr {
      return to_type.fromInt(move(val));
    };
    break;
  case Ptr2Int:
    fn = [&](auto &&val, auto &to_type) -> expr {
      return s.getMemory().ptr2int(val).zextOrTrunc(to_type.bits());
    };
    break;
  case Int2Ptr:
    fn = [&](auto &&val, auto &to_type) -> expr {
      return s.getMemory().int2ptr(val);
    };
    break;
  }

  if (op == BitCast) {
    // NOP: ptr vect -> ptr vect
    if (getType().isVectorType() &&
        getType().getAsAggregateType()->getChild(0).isPtrType())
      return v;

    v = val->getType().toInt(s, move(v));
  }

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto retty = getType().getAsAggregateType();
    auto elems = retty->numElementsConst();

    // bitcast vect elems size may vary, so create a new data type whose
    // element size is aligned with the output vector elem size
    IntType elem_ty("int", retty->bits() / elems);
    VectorType int_ty("vec", elems, elem_ty);
    auto valty = op == BitCast ? &int_ty : val->getType().getAsAggregateType();

    for (unsigned i = 0; i != elems; ++i) {
      unsigned idx = (little_endian && op == BitCast) ? elems - i - 1 : i;
      auto vi = valty->extract(v, idx);
      vals.emplace_back(fn(move(vi.value), retty->getChild(idx)),
                        move(vi.non_poison));
    }
    return retty->aggregateVals(vals);
  }

  // turn poison data into boolean
  if (op == BitCast)
    v.non_poison = v.non_poison == expr::mkInt(-1, v.non_poison);

  return { fn(move(v.value), getType()), move(v.non_poison) };
}

expr ConversionOp::getTypeConstraints(const Function &f) const {
  expr c;
  switch (op) {
  case SExt:
  case ZExt:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceIntOrVectorType() &&
        val->getType().scalarSize().ult(getType().scalarSize());
    break;
  case Trunc:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceIntOrVectorType() &&
        getType().scalarSize().ult(val->getType().scalarSize());
    break;
  case BitCast:
    c = getType().enforceIntOrFloatOrPtrOrVectorType() &&
        val->getType().enforceIntOrFloatOrPtrOrVectorType() &&
        getType().enforcePtrOrVectorType() ==
          val->getType().enforcePtrOrVectorType() &&
        getType().sizeVar() == val->getType().sizeVar();
    break;
  case Ptr2Int:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforcePtrOrVectorType();
    break;
  case Int2Ptr:
    c = getType().enforcePtrOrVectorType() &&
        val->getType().enforceIntOrVectorType();
    break;
  }

  c &= Value::getTypeConstraints();
  if (op != BitCast)
    c &= getType().enforceVectorTypeEquiv(val->getType());
  return c;
}

unique_ptr<Instr> ConversionOp::dup(const string &suffix) const {
  return make_unique<ConversionOp>(getType(), getName() + suffix, *val, op);
}


vector<Value*> FpConversionOp::operands() const {
  return { val };
}

bool FpConversionOp::propagatesPoison() const {
  return true;
}

void FpConversionOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void FpConversionOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case SIntToFP: str = "sitofp "; break;
  case UIntToFP: str = "uitofp "; break;
  case FPToSInt: str = "fptosi "; break;
  case FPToUInt: str = "fptoui "; break;
  case FPExt:    str = "fpext "; break;
  case FPTrunc:  str = "fptrunc "; break;
  case LRInt:    str = "lrint "; break;
  case LRound:   str = "lround "; break;
  }

  os << getName() << " = " << str << *val << print_type(getType(), " to ", "")
     << ", rounding=" << rm;
}

StateValue FpConversionOp::toSMT(State &s) const {
  auto &v = s[*val];
  function<StateValue(const expr &, const Type &, FpRoundingMode)> fn;

  switch (op) {
  case SIntToFP:
    fn = [](auto &val, auto &to_type, auto rm) -> StateValue {
      return
        { val.sint2fp(to_type.getDummyValue(false).value, rm.toSMT()), true };
    };
    break;
  case UIntToFP:
    fn = [](auto &val, auto &to_type, auto rm) -> StateValue {
      return
        { val.uint2fp(to_type.getDummyValue(false).value, rm.toSMT()), true };
    };
    break;
  case FPToSInt:
  case LRInt:
  case LRound:
    fn = [&](auto &val, auto &to_type, auto rm_) -> StateValue {
      expr rm;
      switch (op) {
      case FPToSInt:
        rm = expr::rtz();
        break;
      case LRInt:
        rm = get_fp_rounding(s);
        break;
      case LRound:
        rm = expr::rna();
        break;
      default: UNREACHABLE();
      }
      expr bv  = val.fp2sint(to_type.bits(), rm);
      expr fp2 = bv.sint2fp(val, rm);
      // -0.xx is converted to 0 and then to 0.0, though -0.xx is ok to convert
      expr val_rounded = val.round(rm);
      return { move(bv), val_rounded.isFPZero() || fp2 == val_rounded };
    };
    break;
  case FPToUInt:
    fn = [](auto &val, auto &to_type, auto rm_) -> StateValue {
      expr rm = expr::rtz();
      expr bv  = val.fp2uint(to_type.bits(), rm);
      expr fp2 = bv.uint2fp(val, rm);
      // -0.xx must be converted to 0, not poison.
      expr val_rounded = val.round(rm);
      return { move(bv), val_rounded.isFPZero() || fp2 == val_rounded };
    };
    break;
  case FPExt:
  case FPTrunc:
    fn = [](auto &val, auto &to_type, auto rm) -> StateValue {
      return { val.float2Float(to_type.getDummyValue(false).value, rm.toSMT()),
               true };
    };
    break;
  }

  auto scalar = [&](const StateValue &sv, const Type &to_type) -> StateValue {
    auto [v, np]
      = round_value([&](auto rm) { return fn(sv.value, to_type, rm); }, s, rm);
    return { move(v), sv.non_poison && np };
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = val->getType().getAsAggregateType();
    auto retty = getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(v, i), retty->getChild(i)));
    }
    return retty->aggregateVals(vals);
  }
  return scalar(v, getType());
}

expr FpConversionOp::getTypeConstraints(const Function &f) const {
  expr c;
  switch (op) {
  case SIntToFP:
  case UIntToFP:
    c = getType().enforceFloatOrVectorType() &&
        val->getType().enforceIntOrVectorType();
    break;
  case FPToSInt:
  case FPToUInt:
  case LRInt:
  case LRound:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceFloatOrVectorType();
    break;
  case FPExt:
    c = getType().enforceFloatOrVectorType() &&
        val->getType().enforceFloatOrVectorType() &&
        val->getType().scalarSize().ult(getType().scalarSize());
    break;
  case FPTrunc:
    c = getType().enforceFloatOrVectorType() &&
        val->getType().enforceFloatOrVectorType() &&
        val->getType().scalarSize().ugt(getType().scalarSize());
    break;
  }
  return Value::getTypeConstraints() && c;
}

unique_ptr<Instr> FpConversionOp::dup(const string &suffix) const {
  return
    make_unique<FpConversionOp>(getType(), getName() + suffix, *val, op, rm);
}


vector<Value*> Select::operands() const {
  return { cond, a, b };
}

void Select::rauw(const Value &what, Value &with) {
  RAUW(cond);
  RAUW(a);
  RAUW(b);
}

void Select::print(ostream &os) const {
  os << getName() << " = select " << fmath << *cond << ", " << *a << ", " << *b;
}

StateValue Select::toSMT(State &s) const {
  auto &cv = s[*cond];
  auto &av = s[*a];
  auto &bv = s[*b];

  auto scalar = [&](const auto &a, const auto &b, const auto &c) -> StateValue {
    auto cond = c.value == 1;
    auto identity = [](const expr &x) { return x; };
    StateValue sva = fm_poison(s, a.value, a.non_poison, identity, fmath, true);
    StateValue svb = fm_poison(s, b.value, b.non_poison, identity, fmath, true);
    return { expr::mkIf(cond, sva.value, svb.value),
             c.non_poison && expr::mkIf(cond, sva.non_poison, svb.non_poison) };
  };

  if (auto agg = getType().getAsAggregateType()) {
    vector<StateValue> vals;
    auto cond_agg = cond->getType().getAsAggregateType();

    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (!agg->isPadding(i))
        vals.emplace_back(scalar(agg->extract(av, i), agg->extract(bv, i),
                                 cond_agg ? cond_agg->extract(cv, i) : cv));
    }
    return agg->aggregateVals(vals);
  }
  return scalar(av, bv, cv);
}

expr Select::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         cond->getType().enforceIntOrVectorType(1) &&
         getType().enforceVectorTypeIff(cond->getType()) &&
         (fmath.isNone() ? expr(true) : getType().enforceFloatOrVectorType()) &&
         getType() == a->getType() &&
         getType() == b->getType();
}

unique_ptr<Instr> Select::dup(const string &suffix) const {
  return make_unique<Select>(getType(), getName() + suffix, *cond, *a, *b);
}


void ExtractValue::addIdx(unsigned idx) {
  idxs.emplace_back(idx);
}

vector<Value*> ExtractValue::operands() const {
  return { val };
}

void ExtractValue::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void ExtractValue::print(ostream &os) const {
  os << getName() << " = extractvalue " << *val;
  for (auto idx : idxs) {
    os << ", " << idx;
  }
}

StateValue ExtractValue::toSMT(State &s) const {
  auto v = s[*val];

  Type *type = &val->getType();
  for (auto idx : idxs) {
    auto ty = type->getAsAggregateType();
    v = ty->extract(v, idx);
    type = &ty->getChild(idx);
  }
  return v;
}

expr ExtractValue::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints() &&
           val->getType().enforceAggregateType();

  Type *type = &val->getType();
  unsigned i = 0;
  for (auto idx : idxs) {
    auto ty = type->getAsAggregateType();
    if (!ty) {
      c = false;
      break;
    }
    type = &ty->getChild(idx);

    c &= ty->numElements().ugt(idx);
    if (++i == idxs.size() && idx < ty->numElementsConst())
      c &= ty->getChild(idx) == getType();
  }
  return c;
}

unique_ptr<Instr> ExtractValue::dup(const string &suffix) const {
  auto ret = make_unique<ExtractValue>(getType(), getName() + suffix, *val);
  for (auto idx : idxs) {
    ret->addIdx(idx);
  }
  return ret;
}


void InsertValue::addIdx(unsigned idx) {
  idxs.emplace_back(idx);
}

vector<Value*> InsertValue::operands() const {
  return { val, elt };
}

void InsertValue::rauw(const Value &what, Value &with) {
  RAUW(val);
  RAUW(elt);
}

void InsertValue::print(ostream &os) const {
  os << getName() << " = insertvalue " << *val << ", " << *elt;
  for (auto idx : idxs) {
    os << ", " << idx;
  }
}

static StateValue update_repack(Type *type,
                                const StateValue &val,
                                const StateValue &elem,
                                vector<unsigned> &indices) {
  auto ty = type->getAsAggregateType();
  unsigned cur_idx = indices.back();
  indices.pop_back();
  vector<StateValue> vals;
  for (unsigned i = 0, e = ty->numElementsConst(); i < e; ++i) {
    if (ty->isPadding(i))
      continue;

    auto v = ty->extract(val, i);
    if (i == cur_idx) {
      vals.emplace_back(indices.empty() ?
                        elem :
                        update_repack(&ty->getChild(i), v, elem, indices));
    } else {
      vals.emplace_back(move(v));
    }
  }

  return ty->aggregateVals(vals);
}

StateValue InsertValue::toSMT(State &s) const {
  auto &sv = s[*val];
  auto &elem = s[*elt];

  Type *type = &val->getType();
  vector<unsigned> idxs_reverse(idxs.rbegin(), idxs.rend());
  return update_repack(type, sv, elem, idxs_reverse);
}

expr InsertValue::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints() &&
           val->getType().enforceAggregateType() &&
           val->getType() == getType();

  Type *type = &val->getType();
  unsigned i = 0;
  for (auto idx : idxs) {
    auto ty = type->getAsAggregateType();
    if (!ty)
      return false;

    type = &ty->getChild(idx);

    c &= ty->numElements().ugt(idx);
    if (++i == idxs.size() && idx < ty->numElementsConst())
      c &= ty->getChild(idx) == elt->getType();
  }

  return c;
}

unique_ptr<Instr> InsertValue::dup(const string &suffix) const {
  auto ret = make_unique<InsertValue>(getType(), getName() + suffix, *val, *elt);
  for (auto idx : idxs) {
    ret->addIdx(idx);
  }
  return ret;
}

DEFINE_AS_RETZEROALIGN(FnCall, getMaxAllocSize);
DEFINE_AS_RETZERO(FnCall, getMaxGEPOffset);

bool FnCall::canFree() const {
  return !getAttributes().has(FnAttrs::NoFree);
}

uint64_t FnCall::getMaxAccessSize() const {
  uint64_t sz = attrs.has(FnAttrs::Dereferenceable) ? attrs.derefBytes : 0;
  if (attrs.has(FnAttrs::DereferenceableOrNull))
    sz = max(sz, attrs.derefOrNullBytes);

  for (auto &arg : args) {
    if (arg.second.has(ParamAttrs::Dereferenceable))
      sz = max(sz, arg.second.derefBytes);
    if (arg.second.has(ParamAttrs::DereferenceableOrNull))
      sz = max(sz, arg.second.derefOrNullBytes);
  }
  return sz;
}

FnCall::ByteAccessInfo FnCall::getByteAccessInfo() const {
  // If bytesize is zero, this call does not participate in byte encoding.
  uint64_t bytesize = 0;

#define UPDATE(attr)                                                   \
  do {                                                                 \
    uint64_t sz = 0;                                                   \
    if (attr.has(decay<decltype(attr)>::type::Dereferenceable))        \
      sz = attr.derefBytes;                                            \
    if (attr.has(decay<decltype(attr)>::type::DereferenceableOrNull))  \
      sz = gcd(sz, attr.derefOrNullBytes);                             \
    /* Without align, nothing is guaranteed about the bytesize */      \
    sz = gcd(sz, retattr.align);                                       \
    bytesize = bytesize ? gcd(bytesize, sz) : sz;                      \
  } while (0)

  auto &retattr = getAttributes();
  UPDATE(retattr);

  for (auto &arg : args) {
    if (!arg.first->getType().isPtrType())
      continue;

    UPDATE(arg.second);
    // Pointer arguments without dereferenceable attr don't contribute to the
    // byte size.
    // call f(* dereferenceable(n) align m %p, * %q) is equivalent to a dummy
    // load followed by a function call:
    //   load i<8*n> %p, align m
    //   call f(* %p, * %q)
    // f(%p, %q) does not contribute to the bytesize. After bytesize is fixed,
    // function calls update a memory with the granularity.
  }
  if (bytesize == 0) {
    // No dereferenceable attribute
    return {};
  }

#undef UPDATE

  return ByteAccessInfo::anyType(bytesize);
}


void FnCall::addArg(Value &arg, ParamAttrs &&attrs) {
  args.emplace_back(&arg, move(attrs));
}

vector<Value*> FnCall::operands() const {
  vector<Value*> output;
  transform(args.begin(), args.end(), back_inserter(output),
            [](auto &p){ return p.first; });
  return output;
}

void FnCall::rauw(const Value &what, Value &with) {
  for (auto &arg : args) {
    RAUW(arg.first);
  }
}

void FnCall::print(ostream &os) const {
  if (!isVoid())
    os << getName() << " = ";

  os << "call " << print_type(getType()) << fnName << '(';

  bool first = true;
  for (auto &[arg, attrs] : args) {
    if (!first)
      os << ", ";

    os << attrs << *arg;
    first = false;
  }
  os << ')' << attrs;
}

static void eq_bids(OrExpr &acc, Memory &m, const Type &t,
                    const StateValue &val, const expr &bid) {
  if (auto agg = t.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      eq_bids(acc, m, agg->getChild(i), agg->extract(val, i), bid);
    }
    return;
  }

  if (t.isPtrType()) {
    acc.add(val.non_poison && Pointer(m, val.value).getBid() == bid);
  }
}

static expr ptr_only_args(State &s, const Pointer &p) {
  expr bid = p.getBid();
  auto &m  = s.getMemory();

  OrExpr e;
  for (auto &in : s.getFn().getInputs()) {
    if (hasPtr(in.getType()))
      eq_bids(e, m, in.getType(), s[in], bid);
  }
  return e();
}

static void unpack_inputs(State &s, Value &argv, Type &ty,
                          const ParamAttrs &argflag, bool argmemonly,
                          StateValue value, StateValue value2,
                          vector<StateValue> &inputs,
                          vector<Memory::PtrInput> &ptr_inputs) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      unpack_inputs(s, argv, agg->getChild(i), argflag, argmemonly,
                    agg->extract(value, i), agg->extract(value2, i), inputs,
                    ptr_inputs);
    }
    return;
  }

  auto unpack = [&](StateValue &&value) {
    auto [UB, new_non_poison] = argflag.encode(s, value, ty);
    s.addUB(move(UB));
    value.non_poison = move(new_non_poison);

    if (ty.isPtrType()) {
      if (argmemonly)
        value.non_poison
          &= ptr_only_args(s, Pointer(s.getMemory(), value.value));

      ptr_inputs.emplace_back(move(value),
                              argflag.blockSize,
                              argflag.has(ParamAttrs::NoCapture));
    } else {
      inputs.emplace_back(move(value));
    }
  };
  unpack(move(value));
  unpack(move(value2));
}

static void unpack_ret_ty (vector<Type*> &out_types, Type &ty) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      // Padding is automatically filled with poison
      if (agg->isPadding(i))
        continue;
      unpack_ret_ty(out_types, agg->getChild(i));
    }
  } else {
    out_types.emplace_back(&ty);
  }
}

static StateValue
check_return_value(State &s, StateValue &&val, const Type &ty,
                   const FnAttrs &attrs, bool is_ret_instr) {
  if (ty.isPtrType() && is_ret_instr) {
    Pointer p(s.getMemory(), val.value);
    val.non_poison &= !p.isStackAllocated();
    val.non_poison &= !p.isNocapture();
  }

  auto [UB, new_non_poison] = attrs.encode(s, val, ty);
  s.addUB(move(UB));
  return { move(val.value), move(new_non_poison) };
}

static StateValue
pack_return(State &s, Type &ty, vector<StateValue> &vals, const FnAttrs &attrs,
            unsigned &idx) {
  if (auto agg = ty.getAsAggregateType()) {
    vector<StateValue> vs;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (!agg->isPadding(i))
        vs.emplace_back(pack_return(s, agg->getChild(i), vals, attrs, idx));
    }
    return agg->aggregateVals(vs);
  }

  return check_return_value(s, move(vals[idx++]), ty, attrs, false);
}

StateValue FnCall::toSMT(State &s) const {
  if (approx)
    s.doesApproximation("Unknown libcall: " + fnName);

  vector<StateValue> inputs;
  vector<Memory::PtrInput> ptr_inputs;
  vector<Type*> out_types;
  bool argmemonly_fn   = s.getFn().getFnAttrs().has(FnAttrs::ArgMemOnly);
  bool argmemonly_call = hasAttribute(FnAttrs::ArgMemOnly);

  ostringstream fnName_mangled;
  fnName_mangled << fnName;
  for (auto &[arg, flags] : args) {
    // we duplicate each argument so that undef values are allowed to take
    // different values so we can catch the bug in f(freeze(undef)) -> f(undef)
    StateValue sv, sv2;
    if (flags.poisonImpliesUB()) {
      sv = s.getAndAddPoisonUB(*arg, flags.undefImpliesUB());
      if (flags.undefImpliesUB())
        sv2 = sv;
      else
        sv2 = s.getAndAddPoisonUB(*arg, false);
    } else {
      sv  = s[*arg];
      sv2 = s[*arg];
    }

    unpack_inputs(s, *arg, arg->getType(), flags, argmemonly_fn, move(sv),
                  move(sv2), inputs, ptr_inputs);
    fnName_mangled << '#' << arg->getType();
  }
  fnName_mangled << '!' << getType();
  if (!isVoid())
    unpack_ret_ty(out_types, getType());

  auto check = [&](FnAttrs::Attribute attr) {
    return s.getFn().getFnAttrs().has(attr) && !hasAttribute(attr);
  };

  auto check_implies = [&](FnAttrs::Attribute attr) {
    if (!check(attr))
      return;

    if (argmemonly_call) {
      for (auto &p : ptr_inputs) {
        if (!p.byval) {
          Pointer ptr(s.getMemory(), p.val.value);
          s.addUB(p.val.non_poison.implies(
                    ptr.isLocal() || ptr.isConstGlobal()));
        }
      }
    } else {
      s.addUB(expr(false));
    }
  };

  check_implies(FnAttrs::NoRead);
  check_implies(FnAttrs::NoWrite);
  check_implies(FnAttrs::NoFree);

  // Check attributes that calles must have if caller has them
  if (check(FnAttrs::ArgMemOnly) ||
      check(FnAttrs::NoThrow) ||
      check(FnAttrs::WillReturn) ||
      check(FnAttrs::InaccessibleMemOnly))
    s.addUB(expr(false));

  // can't have both!
  if (attrs.has(FnAttrs::ArgMemOnly) && attrs.has(FnAttrs::InaccessibleMemOnly))
    s.addUB(expr(false));

  unsigned idx = 0;
  auto ret = s.addFnCall(fnName_mangled.str(), move(inputs), move(ptr_inputs),
                         out_types, attrs);

  return isVoid() ? StateValue() : pack_return(s, getType(), ret, attrs, idx);
}

expr FnCall::getTypeConstraints(const Function &f) const {
  // TODO : also need to name each arg type smt var uniquely
  return Value::getTypeConstraints();
}

unique_ptr<Instr> FnCall::dup(const string &suffix) const {
  auto r = make_unique<FnCall>(getType(), getName() + suffix, string(fnName),
                               FnAttrs(attrs));
  r->args = args;
  r->approx = approx;
  return r;
}


InlineAsm::InlineAsm(Type &type, string &&name, const string &asm_str,
                     const string &constraints, FnAttrs &&attrs)
  : FnCall(type, move(name), "asm " + asm_str + ", " + constraints,
           move(attrs)) {}


ICmp::ICmp(Type &type, string &&name, Cond cond, Value &a, Value &b)
  : Instr(type, move(name)), a(&a), b(&b), cond(cond), defined(cond != Any) {
  if (!defined)
    cond_name = getName() + "_cond";
}

expr ICmp::cond_var() const {
  return defined ? expr::mkUInt(cond, 4) : expr::mkVar(cond_name.c_str(), 4);
}

vector<Value*> ICmp::operands() const {
  return { a, b };
}

bool ICmp::propagatesPoison() const {
  return true;
}

bool ICmp::isPtrCmp() const {
  auto &elem_ty = a->getType();
  return elem_ty.isPtrType() ||
      (elem_ty.isVectorType() &&
       elem_ty.getAsAggregateType()->getChild(0).isPtrType());
}

void ICmp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
}

void ICmp::print(ostream &os) const {
  const char *condtxt = nullptr;
  switch (cond) {
  case EQ:  condtxt = "eq "; break;
  case NE:  condtxt = "ne "; break;
  case SLE: condtxt = "sle "; break;
  case SLT: condtxt = "slt "; break;
  case SGE: condtxt = "sge "; break;
  case SGT: condtxt = "sgt "; break;
  case ULE: condtxt = "ule "; break;
  case ULT: condtxt = "ult "; break;
  case UGE: condtxt = "uge "; break;
  case UGT: condtxt = "ugt "; break;
  case Any: condtxt = ""; break;
  }
  os << getName() << " = icmp " << condtxt << *a << ", " << b->getName();
  switch (pcmode) {
  case INTEGRAL: break;
  case PROVENANCE: os << ", use_provenance"; break;
  case OFFSETONLY: os << ", offsetonly"; break;
  }
}

static expr build_icmp_chain(const expr &var,
                             const function<expr(ICmp::Cond)> &fn,
                             ICmp::Cond cond = ICmp::Any,
                             expr last = expr()) {
  auto old_cond = cond;
  cond = ICmp::Cond(cond - 1);

  if (old_cond == ICmp::Any)
    return build_icmp_chain(var, fn, cond, fn(cond));

  auto e = expr::mkIf(var == cond, fn(cond), last);
  return cond == 0 ? e : build_icmp_chain(var, fn, cond, move(e));
}

StateValue ICmp::toSMT(State &s) const {
  auto &a_eval = s[*a];
  auto &b_eval = s[*b];

  function<expr(const expr&, const expr&, Cond)> fn =
      [&](auto &av, auto &bv, Cond cond) {
    switch (cond) {
    case EQ:  return av == bv;
    case NE:  return av != bv;
    case SLE: return av.sle(bv);
    case SLT: return av.slt(bv);
    case SGE: return av.sge(bv);
    case SGT: return av.sgt(bv);
    case ULE: return av.ule(bv);
    case ULT: return av.ult(bv);
    case UGE: return av.uge(bv);
    case UGT: return av.ugt(bv);
    case Any:
      UNREACHABLE();
    }
    UNREACHABLE();
  };

  if (isPtrCmp()) {
    fn = [this, &s, fn](const expr &av, const expr &bv, Cond cond) {
      Pointer lhs(s.getMemory(), av);
      Pointer rhs(s.getMemory(), bv);
      switch (pcmode) {
      case INTEGRAL:
        return fn(lhs.getAddress(), rhs.getAddress(), cond);
      case PROVENANCE:
        assert(cond == EQ || cond == NE);
        return cond == EQ ? lhs == rhs : lhs != rhs;
      case OFFSETONLY:
        return fn(lhs.getOffset(), rhs.getOffset(), cond);
      }
      UNREACHABLE();
    };
  }

  auto scalar = [&](const StateValue &a, const StateValue &b) -> StateValue {
    auto fn2 = [&](Cond c) { return fn(a.value, b.value, c); };
    auto v = cond != Any ? fn2(cond) : build_icmp_chain(cond_var(), fn2);
    return { v.toBVBool(), a.non_poison && b.non_poison };
  };

  auto &elem_ty = a->getType();
  if (auto agg = elem_ty.getAsAggregateType()) {
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(agg->extract(a_eval, i),
                               agg->extract(b_eval, i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return scalar(a_eval, b_eval);
}

expr ICmp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntOrVectorType(1) &&
         getType().enforceVectorTypeEquiv(a->getType()) &&
         a->getType().enforceIntOrPtrOrVectorType() &&
         a->getType() == b->getType();
}

unique_ptr<Instr> ICmp::dup(const string &suffix) const {
  return make_unique<ICmp>(getType(), getName() + suffix, cond, *a, *b);
}


vector<Value*> FCmp::operands() const {
  return { a, b };
}

void FCmp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
}

void FCmp::print(ostream &os) const {
  const char *condtxt = nullptr;
  switch (cond) {
  case OEQ:   condtxt = "oeq "; break;
  case OGT:   condtxt = "ogt "; break;
  case OGE:   condtxt = "oge "; break;
  case OLT:   condtxt = "olt "; break;
  case OLE:   condtxt = "ole "; break;
  case ONE:   condtxt = "one "; break;
  case ORD:   condtxt = "ord "; break;
  case UEQ:   condtxt = "ueq "; break;
  case UGT:   condtxt = "ugt "; break;
  case UGE:   condtxt = "uge "; break;
  case ULT:   condtxt = "ult "; break;
  case ULE:   condtxt = "ule "; break;
  case UNE:   condtxt = "une "; break;
  case UNO:   condtxt = "uno "; break;
  }
  os << getName() << " = fcmp " << fmath << condtxt << *a << ", "
     << b->getName();
}

StateValue FCmp::toSMT(State &s) const {
  auto &a_eval = s[*a];
  auto &b_eval = s[*b];

  auto fn = [&](const auto &a, const auto &b) -> StateValue {
    auto cmp = [&](const expr &a, const expr &b) {
      switch (cond) {
      case OEQ: return a.foeq(b);
      case OGT: return a.fogt(b);
      case OGE: return a.foge(b);
      case OLT: return a.folt(b);
      case OLE: return a.fole(b);
      case ONE: return a.fone(b);
      case ORD: return a.ford(b);
      case UEQ: return a.fueq(b);
      case UGT: return a.fugt(b);
      case UGE: return a.fuge(b);
      case ULT: return a.fult(b);
      case ULE: return a.fule(b);
      case UNE: return a.fune(b);
      case UNO: return a.funo(b);
      }
      UNREACHABLE();
    };
    auto [val, np] = fm_poison(s, a.value, a.non_poison, b.value, b.non_poison,
                               cmp, fmath, true);
    return { val.toBVBool(), move(np) };
  };

  if (auto agg = a->getType().getAsAggregateType()) {
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      vals.emplace_back(fn(agg->extract(a_eval, i), agg->extract(b_eval, i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return fn(a_eval, b_eval);
}

expr FCmp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntOrVectorType(1) &&
         getType().enforceVectorTypeEquiv(a->getType()) &&
         a->getType().enforceFloatOrVectorType() &&
         a->getType() == b->getType();
}

unique_ptr<Instr> FCmp::dup(const string &suffix) const {
  return make_unique<FCmp>(getType(), getName() + suffix, cond, *a, *b, fmath);
}


vector<Value*> Freeze::operands() const {
  return { val };
}

void Freeze::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void Freeze::print(ostream &os) const {
  os << getName() << " = freeze " << print_type(getType()) << val->getName();
}

StateValue Freeze::toSMT(State &s) const {
  auto &v = s[*val];
  s.resetUndefVars();

  auto scalar = [&](auto &v, auto &np, auto &ty) -> StateValue {
    if (np.isTrue())
      return { expr(v), expr(np) };

    StateValue ret_type = ty.getDummyValue(true);
    expr nondet = expr::mkFreshVar("nondet", ret_type.value);
    s.addQuantVar(nondet);
    return { expr::mkIf(np, v, move(nondet)), move(ret_type.non_poison) };
  };

  // TODO: support recursive aggregates

  if (getType().isAggregateType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      if (ty->isPadding(i))
        continue;
      auto vi = ty->extract(v, i);
      vals.emplace_back(scalar(vi.value, vi.non_poison, ty->getChild(i)));
    }
    return ty->aggregateVals(vals);
  }
  return scalar(v.value, v.non_poison, getType());
}

expr Freeze::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val->getType();
}

unique_ptr<Instr> Freeze::dup(const string &suffix) const {
  return make_unique<Freeze>(getType(), getName() + suffix, *val);
}


void Phi::addValue(Value &val, string &&BB_name) {
  values.emplace_back(&val, move(BB_name));
}

void Phi::removeValue(const string &BB_name) {
  for (auto I = values.begin(), E = values.end(); I != E; ++I) {
    if (I->second == BB_name) {
      values.erase(I);
      break;
    }
  }
}

vector<string> Phi::sources() const {
  vector<string> s;
  for (auto &[_, bb] : values) {
    s.emplace_back(bb);
  }
  return s;
}

void Phi::replaceSourceWith(const string &from, const string &to) {
  for (auto &[_, bb] : values) {
    if (bb == from) {
      bb = to;
      break;
    }
  }
}

vector<Value*> Phi::operands() const {
  vector<Value*> v;
  for (auto &[val, bb] : values) {
    v.emplace_back(val);
  }
  return v;
}

void Phi::rauw(const Value &what, Value &with) {
  for (auto &[val, bb] : values) {
    RAUW(val);
  }
}

void Phi::replace(const string &predecessor, Value &newval) {
  for (auto &[val, bb] : values) {
    if (bb == predecessor) {
      val = &newval;
      break;
    }
  }
}

void Phi::print(ostream &os) const {
  os << getName() << " = phi " << print_type(getType());

  bool first = true;
  for (auto &[val, bb] : values) {
    if (!first)
      os << ", ";
    os << "[ " << val->getName() << ", " << bb << " ]";
    first = false;
  }
}

StateValue Phi::toSMT(State &s) const {
  DisjointExpr<StateValue> ret(getType().getDummyValue(false));
  map<Value*, StateValue> cache;

  for (auto &[val, bb] : values) {
    // check if this was a jump from unreachable BB
    if (auto pre = s.jumpCondFrom(s.getFn().getBB(bb))) {
      auto [I, inserted] = cache.try_emplace(val);
      if (inserted)
        I->second = s[*val];
      ret.add(I->second, (*pre)());
    }
  }
  return *ret();
}

expr Phi::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints();
  for (auto &[val, bb] : values) {
    c &= val->getType() == getType();
  }
  return c;
}

unique_ptr<Instr> Phi::dup(const string &suffix) const {
  auto phi = make_unique<Phi>(getType(), getName() + suffix);
  for (auto &[val, bb] : values) {
    phi->addValue(*val, string(bb));
  }
  return phi;
}


const BasicBlock& JumpInstr::target_iterator::operator*() const {
  if (auto br = dynamic_cast<const Branch*>(instr))
    return idx == 0 ? br->getTrue() : *br->getFalse();

  if (auto sw = dynamic_cast<const Switch*>(instr))
    return idx == 0 ? *sw->getDefault() : *sw->getTarget(idx-1).second;

  UNREACHABLE();
}

JumpInstr::target_iterator JumpInstr::it_helper::end() const {
  unsigned idx;
  if (!instr) {
    idx = 0;
  } else if (auto br = dynamic_cast<const Branch*>(instr)) {
    idx = br->getFalse() ? 2 : 1;
  } else if (auto sw = dynamic_cast<const Switch*>(instr)) {
    idx = sw->getNumTargets() + 1;
  } else {
    UNREACHABLE();
  }
  return { instr, idx };
}


void Branch::replaceTargetWith(const BasicBlock *from, const BasicBlock *to) {
  if (dst_true == from)
    dst_true = to;
  if (dst_false == from)
    dst_false = to;
}

vector<Value*> Branch::operands() const {
  if (cond)
    return { cond };
  return {};
}

void Branch::rauw(const Value &what, Value &with) {
  RAUW(cond);
}

void Branch::print(ostream &os) const {
  os << "br ";
  if (cond)
    os << *cond << ", ";
  os << "label " << dst_true->getName();
  if (dst_false)
    os << ", label " << dst_false->getName();
}

StateValue Branch::toSMT(State &s) const {
  if (cond) {
    auto &c = s.getAndAddPoisonUB(*cond, true);
    s.addCondJump(c.value, *dst_true, *dst_false);
  } else {
    s.addJump(*dst_true);
  }
  return {};
}

expr Branch::getTypeConstraints(const Function &f) const {
  if (!cond)
    return true;
  return cond->getType().enforceIntType(1);
}

unique_ptr<Instr> Branch::dup(const string &suffix) const {
  if (dst_false)
    return make_unique<Branch>(*cond, *dst_true, *dst_false);
  return make_unique<Branch>(*dst_true);
}


void Switch::addTarget(Value &val, const BasicBlock &target) {
  targets.emplace_back(&val, &target);
}

void Switch::replaceTargetWith(const BasicBlock *from, const BasicBlock *to) {
  if (default_target == from)
    default_target = to;

  for (auto &[_, bb] : targets) {
    if (bb == from)
      bb = to;
  }
}

vector<Value*> Switch::operands() const {
  vector<Value*> ret = { value };
  for (auto &[val, target] : targets) {
    ret.emplace_back(val);
  }
  return ret;
}

void Switch::rauw(const Value &what, Value &with) {
  RAUW(value);
  for (auto &[val, target] : targets) {
    RAUW(val);
  }
}

void Switch::print(ostream &os) const {
  os << "switch " << *value << ", label " << default_target->getName() << " [\n";
  for (auto &[val, target] : targets) {
    os << "    " << *val << ", label " << target->getName() << '\n';
  }
  os << "  ]";
}

StateValue Switch::toSMT(State &s) const {
  auto &val = s.getAndAddPoisonUB(*value, true);
  expr default_cond(true);

  for (auto &[value_cond, bb] : targets) {
    auto &target = s[*value_cond];
    assert(target.non_poison.isTrue());
    expr cmp = val.value == target.value;
    default_cond &= !cmp;
    s.addJump(move(cmp), *bb);
  }

  s.addJump(move(default_cond), *default_target);
  s.addUB(expr(false));
  return {};
}

expr Switch::getTypeConstraints(const Function &f) const {
  expr typ = value->getType().enforceIntType();
  for (auto &p : targets) {
    typ &= p.first->getType() == value->getType();
  }
  return typ;
}

unique_ptr<Instr> Switch::dup(const string &suffix) const {
  auto sw = make_unique<Switch>(*value, *default_target);
  for (auto &[value_cond, bb] : targets) {
    sw->addTarget(*value_cond, *bb);
  }
  return sw;
}


vector<Value*> Return::operands() const {
  return { val };
}

void Return::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void Return::print(ostream &os) const {
  os << "ret ";
  if (!isVoid())
    os << print_type(getType());
  os << val->getName();
}

static StateValue
check_ret_attributes(State &s, StateValue &&sv, const Type &t,
                     const FnAttrs &attrs) {
  if (auto agg = t.getAsAggregateType()) {
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (agg->isPadding(i))
        continue;
      vals.emplace_back(
        check_ret_attributes(s, agg->extract(sv, i), agg->getChild(i), attrs));
    }
    return agg->aggregateVals(vals);
  }
  return check_return_value(s, move(sv), t, attrs, true);
}

StateValue Return::toSMT(State &s) const {
  StateValue retval;

  auto &attrs = s.getFn().getFnAttrs();
  if (attrs.poisonImpliesUB())
    retval = s.getAndAddPoisonUB(*val, attrs.undefImpliesUB());
  else
    retval = s[*val];

  s.addUB(s.getMemory().checkNocapture());
  retval = check_ret_attributes(s, move(retval), getType(), attrs);

  if (attrs.has(FnAttrs::NoReturn))
    s.addUB(expr(false));

  if (auto *val_returned = s.getFn().getReturnedInput()) {
    // LangRef states that return type must be valid operands for bitcasts,
    // which cannot be aggregate type.
    assert(!dynamic_cast<AggregateType *>(&val->getType()));
    s.addUB(retval == s[*val_returned]);
  }

  s.addReturn(move(retval));
  return {};
}

expr Return::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val->getType() &&
         f.getType() == getType();
}

unique_ptr<Instr> Return::dup(const string &suffix) const {
  return make_unique<Return>(getType(), *val);
}


Assume::Assume(Value &cond, Kind kind)
    : Instr(Type::voidTy, "assume"), args({&cond}), kind(kind) {
  assert(kind == AndNonPoison || kind == IfNonPoison || kind == WellDefined ||
         kind == NonNull);
}

Assume::Assume(vector<Value *> &&args0, Kind kind)
    : Instr(Type::voidTy, "assume"), args(move(args0)), kind(kind) {
  if (args.size() == 1)
    assert(kind == AndNonPoison || kind == IfNonPoison || kind == WellDefined ||
           kind == NonNull);
  else {
    assert(kind == Align && args.size() == 2);
  }
}

vector<Value*> Assume::operands() const {
  return args;
}

void Assume::rauw(const Value &what, Value &with) {
  for (auto &arg: args)
    RAUW(arg);
}

void Assume::print(ostream &os) const {
  const char *str = nullptr;
  switch (kind) {
  case AndNonPoison: str = "assume "; break;
  case IfNonPoison:  str = "assume_non_poison "; break;
  case WellDefined:  str = "assume_welldefined "; break;
  case Align:        str = "assume_align "; break;
  case NonNull:      str = "assume_nonnull "; break;
  }
  os << str;

  bool first = true;
  for (auto &arg: args) {
    if (!first)
      os << ", ";
    os << *arg;
    first = false;
  }
}

StateValue Assume::toSMT(State &s) const {
  switch (kind) {
  case AndNonPoison: {
    auto &v = s.getAndAddPoisonUB(*args[0]);
    s.addUB(v.value != 0);
    break;
  }
  case IfNonPoison: {
    auto &[v, np] = s[*args[0]];
    s.addUB(np.implies(v != 0));
    break;
  }
  case WellDefined:
    (void)s.getAndAddPoisonUB(*args[0], true);
    break;
  case Align: {
    // assume(ptr, align)
    const auto &vptr = s.getAndAddPoisonUB(*args[0]);
    if (auto align = dynamic_cast<IntConst *>(args[1])) {
      Pointer ptr(s.getMemory(), vptr.value);
      s.addUB(ptr.isAligned(*align->getInt()));
    } else {
      // TODO: add support for non-constant align
      s.addUB(expr());
    }
    break;
  }
  case NonNull: {
    // assume(ptr)
    const auto &vptr = s.getAndAddPoisonUB(*args[0]);
    Pointer ptr(s.getMemory(), vptr.value);
    s.addUB(!ptr.isNull());
    break;
  }
  }
  return {};
}

expr Assume::getTypeConstraints(const Function &f) const {
  switch (kind) {
  case WellDefined:
    return true;
  case AndNonPoison:
  case IfNonPoison:
    return args[0]->getType().enforceIntType();
  case Align:
    return args[0]->getType().enforcePtrType() &&
           args[1]->getType().enforceIntType();
  case NonNull:
    return args[0]->getType().enforcePtrType();
  }
  return {};
}

unique_ptr<Instr> Assume::dup(const string &suffix) const {
  return make_unique<Assume>(vector<Value *>(args), kind);
}


MemInstr::ByteAccessInfo MemInstr::ByteAccessInfo::intOnly(unsigned bytesz) {
  ByteAccessInfo info;
  info.byteSize = bytesz;
  info.hasIntByteAccess = true;
  return info;
}

MemInstr::ByteAccessInfo MemInstr::ByteAccessInfo::anyType(unsigned bytesz) {
  ByteAccessInfo info;
  info.byteSize = bytesz;
  return info;
}

MemInstr::ByteAccessInfo
MemInstr::ByteAccessInfo::get(const Type &t, bool store, unsigned align) {
  bool ptr_access = hasPtr(t);
  ByteAccessInfo info;
  info.hasIntByteAccess = t.enforcePtrOrVectorType().isFalse();
  info.doesPtrStore     = ptr_access && store;
  info.doesPtrLoad      = ptr_access && !store;
  info.byteSize         = gcd(align, getCommonAccessSize(t));
  return info;
}

MemInstr::ByteAccessInfo MemInstr::ByteAccessInfo::full(unsigned byteSize) {
  return { true, true, true, true, byteSize };
}


static void check_can_load(State &s, const expr &p0) {
  auto &attrs = s.getFn().getFnAttrs();
  Pointer p(s.getMemory(), p0);

  if (attrs.has(FnAttrs::NoRead))
    s.addUB(p.isLocal() || p.isConstGlobal());
  else if (attrs.has(FnAttrs::ArgMemOnly))
    s.addUB(p.isLocal() || ptr_only_args(s, p));
}

static void check_can_store(State &s, const expr &p0) {
  if (s.isInitializationPhase())
    return;

  auto &attrs = s.getFn().getFnAttrs();
  Pointer p(s.getMemory(), p0);

  if (attrs.has(FnAttrs::NoWrite))
    s.addUB(p.isLocal());
  else if (attrs.has(FnAttrs::ArgMemOnly))
    s.addUB(p.isLocal() || ptr_only_args(s, p));
}


DEFINE_AS_RETZERO(Alloc, getMaxAccessSize);
DEFINE_AS_RETZERO(Alloc, getMaxGEPOffset);
DEFINE_AS_EMPTYACCESS(Alloc);
DEFINE_AS_RETFALSE(Alloc, canFree);

pair<uint64_t, unsigned> Alloc::getMaxAllocSize() const {
  if (auto bytes = getInt(*size)) {
    if (*bytes && mul) {
      if (auto n = getInt(*mul))
        return { *n * abs(*bytes), align };
      return { UINT64_MAX, align };
    }
    return { *bytes, align };
  }
  return { UINT64_MAX, align };
}

vector<Value*> Alloc::operands() const {
  if (mul)
    return { size, mul };
  return { size };
}

void Alloc::rauw(const Value &what, Value &with) {
  RAUW(size);
  RAUW(mul);
}

void Alloc::print(ostream &os) const {
  os << getName() << " = alloca " << *size;
  if (mul)
    os << " x " << *mul;
  os << ", align " << align;
  if (initially_dead)
    os << ", dead";
}

StateValue Alloc::toSMT(State &s) const {
  auto sz = s.getAndAddPoisonUB(*size, true).value;

  if (mul) {
    auto &mul_e = s.getAndAddPoisonUB(*mul, true).value;

    if (sz.bits() > bits_size_t)
      s.addUB(mul_e == 0 || sz.extract(sz.bits()-1, bits_size_t) == 0);
    sz = sz.zextOrTrunc(bits_size_t);

    if (mul_e.bits() > bits_size_t)
      s.addUB(mul_e.extract(mul_e.bits()-1, bits_size_t) == 0);
    auto m = mul_e.zextOrTrunc(bits_size_t);

    s.addUB(sz.mul_no_uoverflow(m));
    sz = sz * m;
  }

  expr ptr = s.getMemory().alloc(sz, align, Memory::STACK, true, true).first;
  if (initially_dead)
    s.getMemory().free(ptr, true);
  return { move(ptr), true };
}

expr Alloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size->getType().enforceIntType();
}

unique_ptr<Instr> Alloc::dup(const string &suffix) const {
  auto a = make_unique<Alloc>(getType(), getName() + suffix, *size, mul, align);
  if (initially_dead)
    a->markAsInitiallyDead();
  return a;
}


DEFINE_AS_RETZERO(Malloc, getMaxAccessSize);
DEFINE_AS_RETZERO(Malloc, getMaxGEPOffset);
DEFINE_AS_EMPTYACCESS(Malloc);

pair<uint64_t, unsigned> Malloc::getMaxAllocSize() const {
  return { getIntOr(*size, UINT64_MAX), getAlign() };
}

bool Malloc::canFree() const {
  return ptr != nullptr;
}

uint64_t Malloc::getAlign() const {
  return align ? align : heap_block_alignment;
}

vector<Value*> Malloc::operands() const {
  if (!ptr)
    return { size };
  else
    return { ptr, size };
}

void Malloc::rauw(const Value &what, Value &with) {
  RAUW(size);
  RAUW(ptr);
}

void Malloc::print(ostream &os) const {
  os << getName();
  if (!ptr)
    os << " = malloc ";
  else
    os << " = realloc " << *ptr << ", ";
  os << *size;
  if (align)
    os << ", align " << align;
  if (isNonNull)
    os << ", nonnull";
}

StateValue Malloc::toSMT(State &s) const {
  auto &m = s.getMemory();
  auto &[sz, np_size] = s.getAndAddPoisonUB(*size, true);

  expr nonnull = expr::mkBoolVar("malloc_never_fails");
  auto [p_new, allocated]
    = m.alloc(sz, getAlign(), Memory::MALLOC, np_size, nonnull);

  expr nullp = Pointer::mkNullPointer(m)();
  expr ret = expr::mkIf(allocated, p_new, nullp);

  if (!ptr) {
    if (isNonNull) {
      // TODO: In C++ we need to throw an exception if the allocation fails,
      // but exception hasn't been modeled yet
      s.addPre(move(allocated));
      ret = p_new;
    }
  } else {
    auto &[p, np_ptr] = s.getAndAddUndefs(*ptr);
    s.addUB(np_ptr);
    check_can_store(s, p);

    Pointer ptr_old(m, p);
    if (s.getFn().getFnAttrs().has(FnAttrs::NoFree))
      s.addUB(ptr_old.isNull() || ptr_old.isLocal());

    m.copy(ptr_old, Pointer(m, p_new));

    // 1) realloc(ptr, 0) always free the ptr.
    // 2) If allocation failed, we should not free previous ptr.
    m.free(expr::mkIf(sz == 0 || allocated, p, nullp), false);
  }
  return { move(ret), true };
}

expr Malloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size->getType().enforceIntType() &&
         (ptr ? ptr->getType().enforcePtrType() : true);
}

unique_ptr<Instr> Malloc::dup(const string &suffix) const {
  if (ptr)
    return make_unique<Malloc>(getType(), getName() + suffix, *ptr, *size);
  return make_unique<Malloc>(getType(), getName() + suffix, *size, isNonNull);
}


DEFINE_AS_RETZERO(Calloc, getMaxAccessSize);
DEFINE_AS_RETZERO(Calloc, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Calloc, canFree);

pair<uint64_t, unsigned> Calloc::getMaxAllocSize() const {
  if (auto sz = getInt(*size)) {
    if (auto n = getInt(*num))
      return { *sz * *n, getAlign() };
  }
  return { UINT64_MAX, getAlign() };
}

Calloc::ByteAccessInfo Calloc::getByteAccessInfo() const {
  auto info = ByteAccessInfo::intOnly(1);
  if (auto n = getInt(*num))
    if (auto sz = getInt(*size)) {
      info.byteSize = gcd(getAlign(), *n * *sz);
    }
  return info;
}

uint64_t Calloc::getAlign() const {
  return align ? align : heap_block_alignment;
}

vector<Value*> Calloc::operands() const {
  return { num, size };
}

void Calloc::rauw(const Value &what, Value &with) {
  RAUW(num);
  RAUW(size);
}

void Calloc::print(ostream &os) const {
  os << getName() << " = calloc " << *num << ", " << *size;
  if (align)
    os << ", align " << align;
}

StateValue Calloc::toSMT(State &s) const {
  auto &[nm, np_num] = s.getAndAddPoisonUB(*num, true);
  auto &[sz, np_sz] = s.getAndAddPoisonUB(*size, true);

  auto np = np_num && np_sz;
  expr size = nm * sz;
  expr nonnull = expr::mkBoolVar("malloc_never_fails");
  auto &m = s.getMemory();
  auto [p, allocated] = m.alloc(size, getAlign(), Memory::MALLOC,
                                np && nm.mul_no_uoverflow(sz), nonnull);

  m.memset(p, { expr::mkUInt(0, 8), true }, size, getAlign(), {}, false);

  return { expr::mkIf(allocated, p, Pointer::mkNullPointer(m)()), true };
}

expr Calloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         num->getType().enforceIntType() &&
         size->getType().enforceIntType() &&
         num->getType() == size->getType();
}

unique_ptr<Instr> Calloc::dup(const string &suffix) const {
  return make_unique<Calloc>(getType(), getName() + suffix, *num, *size);
}


DEFINE_AS_RETZEROALIGN(StartLifetime, getMaxAllocSize);
DEFINE_AS_RETZERO(StartLifetime, getMaxAccessSize);
DEFINE_AS_RETZERO(StartLifetime, getMaxGEPOffset);
DEFINE_AS_EMPTYACCESS(StartLifetime);
DEFINE_AS_RETFALSE(StartLifetime, canFree);

vector<Value*> StartLifetime::operands() const {
  return { ptr };
}

void StartLifetime::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void StartLifetime::print(ostream &os) const {
  os << "start_lifetime " << *ptr;
}

StateValue StartLifetime::toSMT(State &s) const {
  auto &p = s.getAndAddPoisonUB(*ptr, true).value;
  s.getMemory().startLifetime(p);
  return {};
}

expr StartLifetime::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> StartLifetime::dup(const string &suffix) const {
  return make_unique<StartLifetime>(*ptr);
}


DEFINE_AS_RETZEROALIGN(Free, getMaxAllocSize);
DEFINE_AS_RETZERO(Free, getMaxAccessSize);
DEFINE_AS_RETZERO(Free, getMaxGEPOffset);
DEFINE_AS_EMPTYACCESS(Free);

vector<Value*> Free::operands() const {
  return { ptr };
}

bool Free::canFree() const {
  return true;
}

void Free::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Free::print(ostream &os) const {
  os << "free " << *ptr << (heaponly ? "" : " unconstrained");
}

StateValue Free::toSMT(State &s) const {
  auto &p = s.getAndAddPoisonUB(*ptr, true).value;
  // If not heaponly, don't encode constraints
  s.getMemory().free(p, !heaponly);

  if (s.getFn().getFnAttrs().has(FnAttrs::NoFree) && heaponly) {
    Pointer ptr(s.getMemory(), p);
    s.addUB(ptr.isNull() || ptr.isLocal());
  }

  return {};
}

expr Free::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Free::dup(const string &suffix) const {
  return make_unique<Free>(*ptr, heaponly);
}


void GEP::addIdx(uint64_t obj_size, Value &idx) {
  idxs.emplace_back(obj_size, &idx);
}

DEFINE_AS_RETZEROALIGN(GEP, getMaxAllocSize);
DEFINE_AS_RETZERO(GEP, getMaxAccessSize);
DEFINE_AS_EMPTYACCESS(GEP);
DEFINE_AS_RETFALSE(GEP, canFree);

static unsigned off_used_bits(const Value &v) {
  if (auto c = isCast(ConversionOp::SExt, v))
    return off_used_bits(c->getValue());

  if (auto ty = dynamic_cast<IntType*>(&v.getType()))
    return min(ty->bits(), 64u);

  return 64;
}

uint64_t GEP::getMaxGEPOffset() const {
  uint64_t off = 0;
  for (auto &[mul, v] : getIdxs()) {
    if (mul == 0)
      continue;
    if (mul >= INT64_MAX)
      return UINT64_MAX;

    if (auto n = getInt(*v)) {
      off = add_saturate(off, abs((int64_t)mul * *n));
      continue;
    }

    off = add_saturate(off,
                       mul_saturate(mul,
                                    UINT64_MAX >> (64 - off_used_bits(*v))));
  }
  return off;
}

vector<Value*> GEP::operands() const {
  vector<Value*> v = { ptr };
  for (auto &[sz, idx] : idxs) {
    v.emplace_back(idx);
  }
  return v;
}

void GEP::rauw(const Value &what, Value &with) {
  RAUW(ptr);
  for (auto &[sz, idx] : idxs) {
    RAUW(idx);
  }
}

void GEP::print(ostream &os) const {
  os << getName() << " = gep ";
  if (inbounds)
    os << "inbounds ";
  os << *ptr;

  for (auto &[sz, idx] : idxs) {
    os << ", " << sz << " x " << *idx;
  }
}

StateValue GEP::toSMT(State &s) const {
  auto scalar = [&](const StateValue &ptrval,
                    vector<pair<unsigned, StateValue>> &offsets) -> StateValue {
    Pointer ptr(s.getMemory(), ptrval.value);
    AndExpr non_poison(ptrval.non_poison);

    if (inbounds)
      non_poison.add(ptr.inbounds(true));

    for (auto &[sz, idx] : offsets) {
      auto &[v, np] = idx;
      auto multiplier = expr::mkUInt(sz, bits_for_offset);
      auto val = v.sextOrTrunc(bits_for_offset);
      auto inc = multiplier * val;

      if (inbounds) {
        if (sz != 0)
          non_poison.add(val.sextOrTrunc(v.bits()) == v);
        non_poison.add(multiplier.mul_no_soverflow(val));
        non_poison.add(ptr.addNoOverflow(inc));
      }

#ifndef NDEBUG
      int64_t n;
      if (inc.isInt(n))
        assert(ilog2_ceil(abs(n), true) <= bits_for_offset);
#endif

      ptr += inc;
      non_poison.add(np);

      if (inbounds)
        non_poison.add(ptr.inbounds());
    }
    return { ptr.release(), non_poison() };
  };

  if (auto aty = getType().getAsAggregateType()) {
    vector<StateValue> vals;
    auto &ptrval = s[*ptr];
    bool ptr_isvect = ptr->getType().isVectorType();

    for (unsigned i = 0, e = aty->numElementsConst(); i != e; ++i) {
      vector<pair<unsigned, StateValue>> offsets;
      for (auto &[sz, idx] : idxs) {
        if (auto idx_aty = idx->getType().getAsAggregateType())
          offsets.emplace_back(sz, idx_aty->extract(s[*idx], i));
        else
          offsets.emplace_back(sz, s[*idx]);
      }
      vals.emplace_back(scalar(ptr_isvect ? aty->extract(ptrval, i) :
                               (i == 0 ? ptrval : s[*ptr]), offsets));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  vector<pair<unsigned, StateValue>> offsets;
  for (auto &[sz, idx] : idxs)
    offsets.emplace_back(sz, s[*idx]);
  return scalar(s[*ptr], offsets);
}

expr GEP::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints() &&
           getType().enforceVectorTypeIff(ptr->getType()) &&
           getType().enforcePtrOrVectorType();
  for (auto &[sz, idx] : idxs) {
    // It is allowed to have non-vector idx with vector pointer operand
    c &= idx->getType().enforceIntOrVectorType() &&
          getType().enforceVectorTypeIff(idx->getType());
  }
  return c;
}

unique_ptr<Instr> GEP::dup(const string &suffix) const {
  auto dup = make_unique<GEP>(getType(), getName() + suffix, *ptr, inbounds);
  for (auto &[sz, idx] : idxs) {
    dup->addIdx(sz, *idx);
  }
  return dup;
}


DEFINE_AS_RETZEROALIGN(Load, getMaxAllocSize);
DEFINE_AS_RETZERO(Load, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Load, canFree);

uint64_t Load::getMaxAccessSize() const {
  return Memory::getStoreByteSize(getType());
}

Load::ByteAccessInfo Load::getByteAccessInfo() const {
  return ByteAccessInfo::get(getType(), false, align);
}

vector<Value*> Load::operands() const {
  return { ptr };
}

void Load::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Load::print(ostream &os) const {
  os << getName() << " = load " << getType() << ", " << *ptr
     << ", align " << align;
}

StateValue Load::toSMT(State &s) const {
  auto &p = s.getAndAddPoisonUB(*ptr, true).value;
  check_can_load(s, p);
  auto [sv, ub] = s.getMemory().load(p, getType(), align);
  s.addUB(move(ub));
  return sv;
}

expr Load::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Load::dup(const string &suffix) const {
  return make_unique<Load>(getType(), getName() + suffix, *ptr, align);
}


DEFINE_AS_RETZEROALIGN(Store, getMaxAllocSize);
DEFINE_AS_RETZERO(Store, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Store, canFree);

uint64_t Store::getMaxAccessSize() const {
  return Memory::getStoreByteSize(val->getType());
}

Store::ByteAccessInfo Store::getByteAccessInfo() const {
  return ByteAccessInfo::get(val->getType(), true, align);
}

vector<Value*> Store::operands() const {
  return { val, ptr };
}

void Store::rauw(const Value &what, Value &with) {
  RAUW(val);
  RAUW(ptr);
}

void Store::print(ostream &os) const {
  os << "store " << *val << ", " << *ptr << ", align " << align;
}

StateValue Store::toSMT(State &s) const {
  // skip large initializers. FIXME: this should be moved to memory so it can
  // fold subsequent trivial loads
  if (s.isInitializationPhase() &&
      Memory::getStoreByteSize(val->getType()) / (bits_byte / 8) > 128) {
    s.doesApproximation("Large constant initializer removed");
    return {};
  }

  auto &p = s.getAndAddPoisonUB(*ptr, true).value;
  check_can_store(s, p);
  auto &v = s[*val];
  s.getMemory().store(p, v, val->getType(), align, s.getUndefVars());
  return {};
}

expr Store::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Store::dup(const string &suffix) const {
  return make_unique<Store>(*ptr, *val, align);
}


DEFINE_AS_RETZEROALIGN(Memset, getMaxAllocSize);
DEFINE_AS_RETZERO(Memset, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Memset, canFree);

uint64_t Memset::getMaxAccessSize() const {
  return getIntOr(*bytes, UINT64_MAX);
}

Memset::ByteAccessInfo Memset::getByteAccessInfo() const {
  unsigned byteSize = 1;
  if (auto bs = getInt(*bytes))
    byteSize = gcd(align, *bs);
  return ByteAccessInfo::intOnly(byteSize);
}

vector<Value*> Memset::operands() const {
  return { ptr, val, bytes };
}

void Memset::rauw(const Value &what, Value &with) {
  RAUW(ptr);
  RAUW(val);
  RAUW(bytes);
}

void Memset::print(ostream &os) const {
  os << "memset " << *ptr << " align " << align << ", " << *val
     << ", " << *bytes;
}

StateValue Memset::toSMT(State &s) const {
  auto &vbytes = s.getAndAddPoisonUB(*bytes, true).value;

  uint64_t n;
  expr vptr;
  if (vbytes.isUInt(n) && n > 0) {
    vptr = s.getAndAddPoisonUB(*ptr, true).value;
  } else {
    auto &sv_ptr = s[*ptr];
    auto &sv_ptr2 = s[*ptr];
    // can't be poison even if bytes=0 as address must be aligned regardless
    s.addUB(sv_ptr.non_poison);
    s.addUB((vbytes != 0).implies(sv_ptr.value == sv_ptr2.value));
    vptr = sv_ptr.value;
  }
  check_can_store(s, vptr);
  s.getMemory().memset(vptr, s[*val].zextOrTrunc(8), vbytes, align,
                       s.getUndefVars());
  return {};
}

expr Memset::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType() &&
         val->getType().enforceIntType() &&
         bytes->getType().enforceIntType();
}

unique_ptr<Instr> Memset::dup(const string &suffix) const {
  return make_unique<Memset>(*ptr, *val, *bytes, align);
}


DEFINE_AS_RETZEROALIGN(FillPoison, getMaxAllocSize);
DEFINE_AS_RETZERO(FillPoison, getMaxGEPOffset);
DEFINE_AS_RETFALSE(FillPoison, canFree);

uint64_t FillPoison::getMaxAccessSize() const {
  return getGlobalVarSize(ptr);
}

FillPoison::ByteAccessInfo FillPoison::getByteAccessInfo() const {
  return ByteAccessInfo::intOnly(1);
}

vector<Value*> FillPoison::operands() const {
  return { ptr };
}

void FillPoison::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void FillPoison::print(ostream &os) const {
  os << "fillpoison " << *ptr;
}

StateValue FillPoison::toSMT(State &s) const {
  auto &vptr = s.getAndAddPoisonUB(*ptr, true).value;
  Memory &m = s.getMemory();
  m.fillPoison(Pointer(m, vptr).getBid());
  return {};
}

expr FillPoison::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> FillPoison::dup(const string &suffix) const {
  return make_unique<FillPoison>(*ptr);
}


DEFINE_AS_RETZEROALIGN(Memcpy, getMaxAllocSize);
DEFINE_AS_RETZERO(Memcpy, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Memcpy, canFree);

uint64_t Memcpy::getMaxAccessSize() const {
  return getIntOr(*bytes, UINT64_MAX);
}

Memcpy::ByteAccessInfo Memcpy::getByteAccessInfo() const {
#if 0
  if (auto bytes = get_int(i->getBytes()))
    byteSize = gcd(gcd(i->getSrcAlign(), i->getDstAlign()), *bytes);
#endif
  // FIXME: memcpy doesn't have multi-byte support
  // Memcpy does not have sub-byte access, unless the sub-byte type appears
  // at other instructions
  auto info = ByteAccessInfo::full(1);
  info.observesAddresses = false;
  return info;
}

vector<Value*> Memcpy::operands() const {
  return { dst, src, bytes };
}

void Memcpy::rauw(const Value &what, Value &with) {
  RAUW(dst);
  RAUW(src);
  RAUW(bytes);
}

void Memcpy::print(ostream &os) const {
  os << (move ? "memmove " : "memcpy ") << *dst  << " align " << align_dst
     << ", " << *src << " align " << align_src << ", " << *bytes;
}

StateValue Memcpy::toSMT(State &s) const {
  auto &vbytes = s.getAndAddPoisonUB(*bytes, true).value;

  uint64_t n;
  expr vsrc, vdst;
  if (vbytes.isUInt(n) && n > 0) {
    vdst = s.getAndAddPoisonUB(*dst, true).value;
    vsrc = s.getAndAddPoisonUB(*src, true).value;
  } else {
    auto &sv_dst = s[*dst];
    auto &sv_dst2 = s[*dst];
    auto &sv_src = s[*src];
    auto &sv_src2 = s[*src];
    s.addUB((vbytes != 0).implies(sv_dst.non_poison && sv_src.non_poison &&
        (sv_dst.value == sv_dst2.value && sv_src.value == sv_src2.value)));
    vdst = sv_dst.value;
    vsrc = sv_src.value;
  }

  if (vbytes.bits() > bits_size_t)
    s.addUB(
      vbytes.ule(expr::IntUMax(bits_size_t).zext(vbytes.bits() - bits_size_t)));

  check_can_load(s, vsrc);
  check_can_store(s, vdst);
  s.getMemory().memcpy(vdst, vsrc, vbytes, align_dst, align_src, move);
  return {};
}

expr Memcpy::getTypeConstraints(const Function &f) const {
  return dst->getType().enforcePtrType() &&
         dst->getType().enforcePtrType() &&
         bytes->getType().enforceIntType();
}

unique_ptr<Instr> Memcpy::dup(const string &suffix) const {
  return make_unique<Memcpy>(*dst, *src, *bytes, align_dst, align_src, move);
}



DEFINE_AS_RETZEROALIGN(Memcmp, getMaxAllocSize);
DEFINE_AS_RETZERO(Memcmp, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Memcmp, canFree);

uint64_t Memcmp::getMaxAccessSize() const {
  return getIntOr(*num, UINT64_MAX);
}

Memcmp::ByteAccessInfo Memcmp::getByteAccessInfo() const {
  auto info = ByteAccessInfo::anyType(1);
  info.observesAddresses = true;
  return info;
}

vector<Value*> Memcmp::operands() const {
  return { ptr1, ptr2, num };
}

void Memcmp::rauw(const Value &what, Value &with) {
  RAUW(ptr1);
  RAUW(ptr2);
  RAUW(num);
}

void Memcmp::print(ostream &os) const {
  os << getName() << " = " << (is_bcmp ? "bcmp " : "memcmp ") << *ptr1
     << ", " << *ptr2 << ", " << *num;
}

StateValue Memcmp::toSMT(State &s) const {
  auto &[vptr1, np1] = s[*ptr1];
  auto &[vptr2, np2] = s[*ptr2];
  auto &vnum = s.getAndAddPoisonUB(*num).value;
  s.addUB((vnum != 0).implies(np1 && np2));

  check_can_load(s, vptr1);
  check_can_load(s, vptr2);

  Pointer p1(s.getMemory(), vptr1), p2(s.getMemory(), vptr2);
  // memcmp can be optimized to load & icmps, and it requires this
  // dereferenceability check of vnum.
  s.addUB(p1.isDereferenceable(vnum, 1, false));
  s.addUB(p2.isDereferenceable(vnum, 1, false));

  expr zero = expr::mkUInt(0, 32);

  expr result_var, result_var_neg;
  if (is_bcmp) {
    result_var = expr::mkFreshVar("bcmp_nonzero", zero);
    s.addPre(result_var != zero);
    s.addQuantVar(result_var);
  } else {
    auto z31 = expr::mkUInt(0, 31);
    result_var = expr::mkFreshVar("memcmp_nonzero", z31);
    s.addPre(result_var != z31);
    s.addQuantVar(result_var);
    result_var = expr::mkUInt(0, 1).concat(result_var);

    result_var_neg = expr::mkFreshVar("memcmp", z31);
    s.addQuantVar(result_var_neg);
    result_var_neg = expr::mkUInt(1, 1).concat(result_var_neg);
  }

  auto ith_exec =
      [&, this](unsigned i, bool is_last) -> tuple<expr, expr, AndExpr, expr> {
    assert(bits_byte == 8); // TODO: remove constraint
    auto val1 = s.getMemory().raw_load(p1 + i);
    auto val2 = s.getMemory().raw_load(p2 + i);
    expr is_ptr1 = val1.isPtr();
    expr is_ptr2 = val2.isPtr();

    expr result_neq;
    if (is_bcmp) {
      result_neq = result_var;
    } else {
      expr pos
        = mkIf_fold(is_ptr1,
                    val1.ptr().getAddress().uge(val2.ptr().getAddress()),
                    val1.nonptrValue().uge(val2.nonptrValue()));
      result_neq = expr::mkIf(pos, result_var, result_var_neg);
    }

    // allow null <-> 0 comparison
    expr val_eq =
      (is_ptr1 == is_ptr2 &&
       mkIf_fold(is_ptr1,
                 val1.ptr().getAddress() == val2.ptr().getAddress(),
                 val1.nonptrValue() == val2.nonptrValue())) ||
      (val1.isZero() && val2.isZero());

    expr np
      = (is_ptr1 == is_ptr2 || val1.isZero() || val2.isZero()) &&
        !val1.isPoison() && !val2.isPoison();

    return { expr::mkIf(val_eq, zero, result_neq),
             move(np), {},
             val_eq && vnum.uge(i + 2) };
  };
  auto [val, np, ub]
    = LoopLikeFunctionApproximator(ith_exec).encode(s, memcmp_unroll_cnt);
  return { expr::mkIf(vnum == 0, zero, move(val)), (vnum != 0).implies(np) };
}

expr Memcmp::getTypeConstraints(const Function &f) const {
  return ptr1->getType().enforcePtrType() &&
         ptr2->getType().enforcePtrType() &&
         num->getType().enforceIntType();
}

unique_ptr<Instr> Memcmp::dup(const string &suffix) const {
  return make_unique<Memcmp>(getType(), getName() + suffix, *ptr1, *ptr2, *num,
                             is_bcmp);
}


DEFINE_AS_RETZEROALIGN(Strlen, getMaxAllocSize);
DEFINE_AS_RETZERO(Strlen, getMaxGEPOffset);
DEFINE_AS_RETFALSE(Strlen, canFree);

uint64_t Strlen::getMaxAccessSize() const {
  return getGlobalVarSize(ptr);
}

MemInstr::ByteAccessInfo Strlen::getByteAccessInfo() const {
  return ByteAccessInfo::intOnly(1); /* strlen raises UB on ptr bytes */
}

vector<Value*> Strlen::operands() const {
  return { ptr };
}

void Strlen::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Strlen::print(ostream &os) const {
  os << getName() << " = strlen " << *ptr;
}

StateValue Strlen::toSMT(State &s) const {
  auto &eptr = s.getAndAddPoisonUB(*ptr, true).value;
  check_can_load(s, eptr);

  Pointer p(s.getMemory(), eptr);
  Type &ty = getType();

  auto ith_exec =
      [&s, &p, &ty](unsigned i, bool _) -> tuple<expr, expr, AndExpr, expr> {
    AndExpr ub;
    auto [val, ub_load] = s.getMemory().load((p + i)(), IntType("i8", 8), 1);
    ub.add(move(ub_load));
    ub.add(move(val.non_poison));
    return { expr::mkUInt(i, ty.bits()), true, move(ub), val.value != 0 };
  };
  auto [val, _, ub]
    = LoopLikeFunctionApproximator(ith_exec).encode(s, strlen_unroll_cnt);
  s.addUB(move(ub));
  return { move(val), true };
}

expr Strlen::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr->getType().enforcePtrType() &&
         getType().enforceIntType();
}

unique_ptr<Instr> Strlen::dup(const string &suffix) const {
  return make_unique<Strlen>(getType(), getName() + suffix, *ptr);
}


vector<Value*> VaStart::operands() const {
  return { ptr };
}

void VaStart::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void VaStart::print(ostream &os) const {
  os << "call void @llvm.va_start(" << *ptr << ')';
}

StateValue VaStart::toSMT(State &s) const {
  s.addUB(expr(s.getFn().isVarArgs()));

  auto &data  = s.getVarArgsData();
  auto &raw_p = s.getAndAddPoisonUB(*ptr, true).value;

  expr zero     = expr::mkUInt(0, VARARG_BITS);
  expr num_args = expr::mkVar("num_va_args", VARARG_BITS);

  // just in case there's already a pointer there
  OrExpr matched_one;
  for (auto &[ptr, entry] : data) {
    // FIXME. if entry.alive => memory leak (though not UB). detect this
    expr eq = ptr == raw_p;
    entry.alive      |= eq;
    entry.next_arg    = expr::mkIf(eq, zero, entry.next_arg);
    entry.num_args    = expr::mkIf(eq, num_args, entry.num_args);
    entry.is_va_start = expr::mkIf(eq, true, entry.is_va_start);
    matched_one.add(move(eq));
  }

  Pointer ptr(s.getMemory(), raw_p);
  s.addUB(ptr.isBlockAlive());
  s.addUB(ptr.blockSize().uge(4)); // FIXME: this is target dependent

  // alive, next_arg, num_args, is_va_start, active
  data.try_emplace(raw_p, expr(true), move(zero), move(num_args), expr(true),
                   !matched_one());

  return {};
}

expr VaStart::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> VaStart::dup(const string &suffix) const {
  return make_unique<VaStart>(*ptr);
}


vector<Value*> VaEnd::operands() const {
  return { ptr };
}

void VaEnd::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void VaEnd::print(ostream &os) const {
  os << "call void @llvm.va_end(" << *ptr << ')';
}

template <typename D>
static void ensure_varargs_ptr(D &data, State &s, const expr &arg_ptr) {
  OrExpr matched_one;
  for (auto &[ptr, entry] : data) {
    matched_one.add(ptr == arg_ptr);
  }

  expr matched = matched_one();
  if (matched.isTrue())
    return;

  // Insert a new entry in case there was none before.
  // This might be a ptr passed as argument (va_start in the callee).
  s.addUB(matched || !Pointer(s.getMemory(), arg_ptr).isLocal());

  expr zero = expr::mkUInt(0, VARARG_BITS);
  ENSURE(data.try_emplace(arg_ptr,
                          expr::mkUF("vararg_alive", { arg_ptr }, false),
                          expr(zero), // = next_arg
                          expr::mkUF("vararg_num_args", { arg_ptr }, zero),
                          expr(false), // = is_va_start
                          !matched).second);
}

StateValue VaEnd::toSMT(State &s) const {
  auto &data  = s.getVarArgsData();
  auto &raw_p = s.getAndAddPoisonUB(*ptr, true).value;

  s.addUB(Pointer(s.getMemory(), raw_p).isBlockAlive());

  ensure_varargs_ptr(data, s, raw_p);

  for (auto &[ptr, entry] : data) {
    expr eq = ptr == raw_p;
    s.addUB((eq && entry.active).implies(entry.alive));
    entry.alive &= !eq;
  }
  return {};
}

expr VaEnd::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> VaEnd::dup(const string &suffix) const {
  return make_unique<VaEnd>(*ptr);
}


vector<Value*> VaCopy::operands() const {
  return { dst, src };
}

void VaCopy::rauw(const Value &what, Value &with) {
  RAUW(dst);
  RAUW(src);
}

void VaCopy::print(ostream &os) const {
  os << "call void @llvm.va_copy(" << *dst << ", " << *src << ')';
}

StateValue VaCopy::toSMT(State &s) const {
  auto &data = s.getVarArgsData();
  auto &dst_raw = s.getAndAddPoisonUB(*dst, true).value;
  auto &src_raw = s.getAndAddPoisonUB(*src, true).value;
  Pointer dst(s.getMemory(), dst_raw);
  Pointer src(s.getMemory(), src_raw);

  s.addUB(dst.isBlockAlive());
  s.addUB(src.isBlockAlive());
  s.addUB(dst.blockSize() == src.blockSize());

  ensure_varargs_ptr(data, s, src_raw);

  DisjointExpr<expr> next_arg, num_args, is_va_start;
  for (auto &[ptr, entry] : data) {
    expr select = entry.active && ptr == src_raw;
    s.addUB(select.implies(entry.alive));

    next_arg.add(entry.next_arg, select);
    num_args.add(entry.num_args, select);
    is_va_start.add(entry.is_va_start, move(select));

    // kill aliases
    entry.active &= ptr != dst_raw;
  }

  // FIXME: dst should be empty or we have a mem leak
  // alive, next_arg, num_args, is_va_start, active
  data[dst_raw] = { true, *next_arg(), *num_args(), *is_va_start(), true };

  return {};
}

expr VaCopy::getTypeConstraints(const Function &f) const {
  return dst->getType().enforcePtrType() &&
         src->getType().enforcePtrType();
}

unique_ptr<Instr> VaCopy::dup(const string &suffix) const {
  return make_unique<VaCopy>(*dst, *src);
}


vector<Value*> VaArg::operands() const {
  return { ptr };
}

void VaArg::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void VaArg::print(ostream &os) const {
  os << getName() << " = va_arg " << *ptr << ", " << getType();
}

StateValue VaArg::toSMT(State &s) const {
  auto &data  = s.getVarArgsData();
  auto &raw_p = s.getAndAddPoisonUB(*ptr, true).value;

  s.addUB(Pointer(s.getMemory(), raw_p).isBlockAlive());

  ensure_varargs_ptr(data, s, raw_p);

  DisjointExpr<StateValue> ret(StateValue{});
  expr value_kind = getType().getDummyValue(false).value;
  expr one = expr::mkUInt(1, VARARG_BITS);

  for (auto &[ptr, entry] : data) {
    string type = getType().toString();
    string arg_name = "va_arg_" + type;
    string arg_in_name = "va_arg_in_" + type;
    StateValue val = {
      expr::mkIf(entry.is_va_start,
                 expr::mkUF(arg_name.c_str(), { entry.next_arg }, value_kind),
                 expr::mkUF(arg_in_name.c_str(), { ptr, entry.next_arg },
                            value_kind)),
      expr::mkIf(entry.is_va_start,
                 expr::mkUF("va_arg_np", { entry.next_arg }, true),
                 expr::mkUF("va_arg_np_in", { ptr, entry.next_arg }, true))
    };
    expr eq = ptr == raw_p;
    expr select = entry.active && eq;
    ret.add(move(val), select);

    expr next_arg = entry.next_arg + one;
    s.addUB(select.implies(entry.alive && entry.num_args.uge(next_arg)));
    entry.next_arg = expr::mkIf(eq, next_arg, entry.next_arg);
  }

  return *ret();
}

expr VaArg::getTypeConstraints(const Function &f) const {
  return getType().enforceScalarType() &&
         ptr->getType().enforcePtrType();
}

unique_ptr<Instr> VaArg::dup(const string &suffix) const {
  return make_unique<VaArg>(getType(), getName() + suffix, *ptr);
}


vector<Value*> ExtractElement::operands() const {
  return { v, idx };
}

void ExtractElement::rauw(const Value &what, Value &with) {
  RAUW(v);
  RAUW(idx);
}

void ExtractElement::print(ostream &os) const {
  os << getName() << " = extractelement " << *v << ", " << *idx;
}

StateValue ExtractElement::toSMT(State &s) const {
  auto &[iv, ip] = s[*idx];
  auto vty = static_cast<const VectorType*>(v->getType().getAsAggregateType());
  expr inbounds = iv.ult(vty->numElementsConst());
  auto [rv, rp] = vty->extract(s[*v], iv);
  return { move(rv), ip && inbounds && rp };
}

expr ExtractElement::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         v->getType().enforceVectorType([&](auto &ty)
                                        { return ty == getType(); }) &&
         idx->getType().enforceIntType();
}

unique_ptr<Instr> ExtractElement::dup(const string &suffix) const {
  return make_unique<ExtractElement>(getType(), getName() + suffix, *v, *idx);
}


vector<Value*> InsertElement::operands() const {
  return { v, e, idx };
}

void InsertElement::rauw(const Value &what, Value &with) {
  RAUW(v);
  RAUW(e);
  RAUW(idx);
}

void InsertElement::print(ostream &os) const {
  os << getName() << " = insertelement " << *v << ", " << *e << ", " << *idx;
}

StateValue InsertElement::toSMT(State &s) const {
  auto &[iv, ip] = s[*idx];
  auto vty = static_cast<const VectorType*>(v->getType().getAsAggregateType());
  expr inbounds = iv.ult(vty->numElementsConst());
  auto [rv, rp] = vty->update(s[*v], s[*e], iv);
  return { move(rv), expr::mkIf(ip && inbounds, move(rp),
                                vty->getDummyValue(false).non_poison) };
}

expr InsertElement::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == v->getType() &&
         v->getType().enforceVectorType([&](auto &ty)
                                        { return ty == e->getType(); }) &&
         idx->getType().enforceIntType();
}

unique_ptr<Instr> InsertElement::dup(const string &suffix) const {
  return make_unique<InsertElement>(getType(), getName() + suffix,
                                    *v, *e, *idx);
}


vector<Value*> ShuffleVector::operands() const {
  return { v1, v2 };
}

void ShuffleVector::rauw(const Value &what, Value &with) {
  RAUW(v1);
  RAUW(v2);
}

void ShuffleVector::print(ostream &os) const {
  os << getName() << " = shufflevector " << *v1 << ", " << *v2;
  for (auto m : mask)
    os << ", " << m;
}

StateValue ShuffleVector::toSMT(State &s) const {
  auto vty = v1->getType().getAsAggregateType();
  auto sz = vty->numElementsConst();
  vector<StateValue> vals;

  for (auto m : mask) {
    if (m >= 2 * sz) {
      vals.emplace_back(UndefValue(vty->getChild(0)).toSMT(s).value, true);
    } else {
      auto *vect = &s[m < sz ? *v1 : *v2];
      vals.emplace_back(vty->extract(*vect, m % sz));
    }
  }

  return getType().getAsAggregateType()->aggregateVals(vals);
}

expr ShuffleVector::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceVectorTypeSameChildTy(v1->getType()) &&
         getType().getAsAggregateType()->numElements() == mask.size() &&
         v1->getType().enforceVectorType() &&
         v1->getType() == v2->getType();
}

unique_ptr<Instr> ShuffleVector::dup(const string &suffix) const {
  return make_unique<ShuffleVector>(getType(), getName() + suffix,
                                    *v1, *v2, mask);
}


const ConversionOp* isCast(ConversionOp::Op op, const Value &v) {
  auto c = dynamic_cast<const ConversionOp*>(&v);
  return (c && c->getOp() == op) ? c : nullptr;
}

bool hasNoSideEffects(const Instr &i) {
  return isNoOp(i) ||
         dynamic_cast<const ExtractValue*>(&i) ||
         dynamic_cast<const GEP*>(&i) ||
         dynamic_cast<const ShuffleVector*>(&i);
}

Value* isNoOp(const Value &v) {
  if (auto *c = isCast(ConversionOp::BitCast, v))
    return &c->getValue();

  if (auto gep = dynamic_cast<const GEP*>(&v))
    return gep->getMaxGEPOffset() == 0 ? &gep->getPtr() : nullptr;

  if (auto unop = dynamic_cast<const UnaryOp*>(&v)) {
    if (unop->getOp() == UnaryOp::Copy)
      return &unop->getValue();
  }

  return nullptr;
}
}
