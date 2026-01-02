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
#include "util/config.h"
#include <algorithm>
#include <functional>
#include <numeric>
#include <sstream>

using namespace smt;
using namespace util;
using namespace std;

#define DEFINE_AS_RETZERO(cls, method) \
  uint64_t cls::method() const { return 0; }
#define DEFINE_AS_RETZEROALIGN(cls, method) \
  pair<uint64_t, uint64_t> cls::method() const { return { 0, 1 }; }
#define DEFINE_AS_RETFALSE(cls, method) \
  bool cls::method() const { return false; }
#define DEFINE_AS_EMPTYACCESS(cls) \
  MemInstr::ByteAccessInfo cls::getByteAccessInfo() const { return {}; }

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

  LoopLikeFunctionApproximator(fn_t ith_exec) : ith_exec(std::move(ith_exec)) {}

  // (value, nonpoison, UB)
  tuple<expr, expr, expr> encode(IR::State &s, unsigned unroll_cnt) {
    AndExpr prefix;
    return _loop(s, prefix, 0, unroll_cnt);
  }

  // (value, nonpoison, UB)
  tuple<expr, expr, expr> _loop(IR::State &s, AndExpr &prefix, unsigned i,
                                unsigned unroll_cnt) {
    bool is_last = i >= unroll_cnt - 1;
    auto [res_i, np_i, ub_i, continue_i] = ith_exec(i, is_last);
    auto ub = ub_i();
    prefix.add(ub_i);

    // Keep going if the function is being applied to a constant input
    if (i < 512)
      is_last &= !continue_i.isConst();

    if (is_last)
      s.addPre(prefix().implies(!continue_i));

    if (is_last || continue_i.isFalse() || ub.isFalse() || !s.isViablePath())
      return { std::move(res_i), std::move(np_i), std::move(ub) };

    prefix.add(continue_i);
    auto [val_next, np_next, ub_next] = _loop(s, prefix, i + 1, unroll_cnt);
    return { expr::mkIf(continue_i, std::move(val_next), std::move(res_i)),
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

expr Instr::getTypeConstraints() const {
  UNREACHABLE();
  return {};
}

bool Instr::isTerminator() const {
  return false;
}

BinOp::BinOp(Type &type, string &&name, Value &lhs, Value &rhs, Op op,
             unsigned flags)
  : Instr(type, std::move(name)), lhs(&lhs), rhs(&rhs), op(op), flags(flags) {
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
  case Or:
    assert((flags & Disjoint) == flags);
    break;
  default:
    assert(flags == 0);
    break;
  }
}

vector<Value*> BinOp::operands() const {
  return { lhs, rhs };
}

bool BinOp::propagatesPoison() const {
  return true;
}

bool BinOp::hasSideEffects() const {
  return isDivOrRem();
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
  case UCmp:          str = "ucmp "; break;
  case SCmp:          str = "scmp "; break;
  }

  os << getName() << " = " << str;

  if (flags & NSW)
    os << "nsw ";
  if (flags & NUW)
    os << "nuw ";
  if (flags & Exact)
    os << "exact ";
  if (flags & Disjoint)
    os << "disjoint ";

  if (op == UCmp || op == SCmp)
    os << getType() << ' ';
  os << *lhs << ", " << rhs->getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  // addUB(bp) is not needed because it is registered by getAndAddPoisonUB.
  assert(!bp.isValid() || bp.isTrue());
  s.addGuardableUB(b != 0);
  if (sign)
    s.addGuardableUB((ap && a != expr::IntSMin(b.bits())) ||
                     b != expr::mkInt(-1, b));
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
      return { a + b, std::move(non_poison) };
    };
    break;

  case Sub:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.sub_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.sub_no_uoverflow(b);
      return { a - b, std::move(non_poison) };
    };
    break;

  case Mul:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.mul_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.mul_no_uoverflow(b);
      return { a * b, std::move(non_poison) };
    };
    break;

  case SDiv:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      div_ub(s, a, b, ap, bp, true);
      if (flags & Exact)
        non_poison = a.sdiv_exact(b);
      return { a.sdiv(b), std::move(non_poison) };
    };
    break;

  case UDiv:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      expr non_poison = true;
      div_ub(s, a, b, ap, bp, false);
      if (flags & Exact)
        non_poison &= a.udiv_exact(b);
      return { a.udiv(b), std::move(non_poison) };
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

      return { a << b, std::move(non_poison) };
    };
    break;

  case AShr:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & Exact)
        non_poison &= a.ashr_exact(b);
      return { a.ashr(b), std::move(non_poison) };
    };
    break;

  case LShr:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & Exact)
        non_poison &= a.lshr_exact(b);
      return { a.lshr(b), std::move(non_poison) };
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
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a | b, (flags & Disjoint) ? (a & b) == 0 : true };
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
      return { std::move(v), ap && bp };
    };
    break;

  case Abs:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      return { a.abs(), ap && bp && (b == 0 || a != expr::IntSMin(a.bits())) };
    };
    break;

  case UCmp:
  case SCmp:
    fn = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto &ty = getType();
      uint32_t resBits =
          (ty.isVectorType() ? ty.getAsAggregateType()->getChild(0) : ty)
              .bits();
      return {expr::mkIf(a == b, expr::mkUInt(0, resBits),
                         expr::mkIf(op == UCmp ? a.ult(b) : a.slt(b),
                                    expr::mkInt(-1, resBits),
                                    expr::mkInt(1, resBits))),
              ap && bp};
    };
    break;
  }

  function<pair<StateValue,StateValue>(const expr&, const expr&, const expr&,
                                       const expr&)> zip_op;
  if (vertical_zip) {
    zip_op = [&](auto &a, auto &ap, auto &b, auto &bp) {
      auto [v1, v2] = fn(a, ap, b, bp);
      expr non_poison = ap && bp;
      StateValue sv1(std::move(v1), expr(non_poison));
      return make_pair(std::move(sv1),
                       StateValue(std::move(v2), std::move(non_poison)));
    };
  } else {
    scalar_op = [&](auto &a, auto &ap, auto &b, auto &bp) -> StateValue {
      auto [v, np] = fn(a, ap, b, bp);
      return { std::move(v), ap && bp && np };
    };
  }

  auto &a = s[*lhs];
  auto &b = s.getVal(*rhs, isDivOrRem());

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
        vals1.emplace_back(std::move(v1));
        vals2.emplace_back(std::move(v2));
      }
      vals.emplace_back(val1ty->aggregateVals(vals1));
      vals.emplace_back(val2ty->aggregateVals(vals2));
    } else {
      StateValue tmp;
      auto opty = lhs->getType().getAsAggregateType();
      for (unsigned i = 0, e = opty->numElementsConst(); i != e; ++i) {
        auto ai = opty->extract(a, i);
        const StateValue *bi;
        switch (op) {
        case Abs:
        case Cttz:
        case Ctlz:
          bi = &b;
          break;
        default:
          tmp = opty->extract(b, i);
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
    vals.emplace_back(std::move(v1));
    vals.emplace_back(std::move(v2));
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
  case UCmp:
  case SCmp:
    instrconstr = getType().enforceScalarOrVectorType([&](auto &ty) {
      return ty.enforceIntType() && ty.sizeVar() >= 2;
    }) && getType().enforceVectorTypeEquiv(lhs->getType()) &&
                  lhs->getType().enforceIntOrVectorType() &&
                  lhs->getType() == rhs->getType();
    break;
  default:
    instrconstr = getType().enforceIntOrVectorType() &&
                  getType() == lhs->getType() &&
                  getType() == rhs->getType();
    break;
  }
  return Value::getTypeConstraints() && std::move(instrconstr);
}

unique_ptr<Instr> BinOp::dup(Function &f, const string &suffix) const {
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

bool FpBinOp::hasSideEffects() const {
  return false;
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
  case FMaximumnum: str = "fmaximumnum "; break;
  case FMinimumnum: str = "fminimumnum "; break;
  case CopySign: str = "copysign "; break;
  }
  os << getName() << " = " << str << fmath << *lhs << ", " << rhs->getName();
  if (!rm.isDefault())
    os << ", rounding=" << rm;
  if (!ex.ignore())
    os << ", exceptions=" << ex;
}

static expr fmin_fmax(State &s, const expr &a, const expr &b, const expr &rm,
                      bool min) {
  expr ndet = s.getFreshNondetVar("maxminnondet", true);
  expr cmp = min ? a.fole(b) : a.foge(b);
  return expr::mkIf(a.isNaN(), b,
                    expr::mkIf(b.isNaN(), a,
                               expr::mkIf(a.foeq(b),
                                          expr::mkIf(ndet, a, b),
                                          expr::mkIf(cmp, a, b))));
}

static expr fminimum_fmaximum(State &s, const expr &a, const expr &b,
                              const expr &rm, bool min) {
  expr zpos = expr::mkNumber("0", a), zneg = expr::mkNumber("-0", a);
  expr cmp = min ? a.fole(b) : a.foge(b);
  expr neg_cond = min ? (a.isFPNegative() || b.isFPNegative())
                      : (a.isFPNegative() && b.isFPNegative());
  expr e = expr::mkIf(a.isFPZero() && b.isFPZero(),
                      expr::mkIf(neg_cond, zneg, zpos),
                      expr::mkIf(cmp, a, b));

  return expr::mkIf(a.isNaN(), a, expr::mkIf(b.isNaN(), b, e));
}



static expr any_fp_zero(State &s, const expr &v) {
  expr is_zero = v.isFPZero();
  if (is_zero.isFalse())
    return v;

  // any-fp-zero2(any-fp-zero1(x)) -> any-fp-zero2(x)
  {
    expr cond, neg, negv, val;
    if (v.isIf(cond, neg, val) && neg.isFPNeg(negv) && negv.eq(val)) {
      expr a, b;
      if (cond.isAnd(a, b) && a.isVar() && a.fn_name().starts_with("anyzero") &&
          b.isIsFPZero())
        return any_fp_zero(s, val);
    }
  }

  expr var = s.getFreshNondetVar("anyzero", true);
  return expr::mkIf(var && is_zero, v.fneg(), v);
}

static expr handle_subnormal(const State &s, FPDenormalAttrs::Type attr,
                             expr &&v) {
  auto posz = [&]() {
    return expr::mkIf(v.isFPSubNormal(), expr::mkNumber("0", v), v);
  };
  auto sign = [&]() {
    return expr::mkIf(v.isFPSubNormal(),
                      expr::mkIf(v.isFPNegative(),
                                 expr::mkNumber("-0", v),
                                 expr::mkNumber("0", v)),
                      v);
  };

  switch (attr) {
  case FPDenormalAttrs::IEEE:
    break;
  case FPDenormalAttrs::PositiveZero:
    v = posz();
    break;
  case FPDenormalAttrs::PreserveSign:
    v = sign();
    break;
  case FPDenormalAttrs::Dynamic: {
    auto &mode = s.getFpDenormalMode();
    v = expr::mkIf(mode == FPDenormalAttrs::IEEE,
                   v,
                   expr::mkIf(mode == FPDenormalAttrs::PositiveZero,
                              posz(),
                              sign()));
    break;
  }
  }
  return std::move(v);
}

template <typename T>
static T round_value(const State &s, FpRoundingMode rm, AndExpr &non_poison,
                     const function<T(const expr&)> &fn) {
  if (rm.isDefault())
    return fn(expr::rne());

  auto &var = s.getFpRoundingMode();
  if (!rm.isDynamic()) {
    non_poison.add(var == rm.getMode());
    return fn(rm.toSMT());
  }

  return fn(expr::mkIf(var == FpRoundingMode::RNE, expr::rne(),
            expr::mkIf(var == FpRoundingMode::RNA, expr::rna(),
            expr::mkIf(var == FpRoundingMode::RTP, expr::rtp(),
            expr::mkIf(var == FpRoundingMode::RTN, expr::rtn(),
                       expr::rtz())))));
}

static StateValue fm_poison(State &s, expr a, const expr &ap, expr b,
                            const expr &bp, expr c, const expr &cp,
                            function<expr(const expr&, const expr&,
                                          const expr&, const expr&)> fn,
                            const Type &from_ty, FastMathFlags fmath,
                            FpRoundingMode rm, bool bitwise,
                            bool flags_in_only = false,
                            const Type *to_ty = nullptr, int nary = 3) {
  AndExpr non_poison;
  non_poison.add(ap);
  if (nary >= 2)
    non_poison.add(bp);
  if (nary >= 3)
    non_poison.add(cp);

  if (!from_ty.isFloatType())
    return { fn(a, b, c, {}), non_poison() };

  auto &fpty = *from_ty.getAsFloatType();

  if (fmath.flags & FastMathFlags::NSZ) {
    a = any_fp_zero(s, a);
    if (nary >= 2) {
      b = any_fp_zero(s, b);
      if (nary == 3)
        c = any_fp_zero(s, c);
    }
  }

  expr fp_a = fpty.getFloat(a);
  expr fp_b = fpty.getFloat(b);
  expr fp_c = fpty.getFloat(c);

  if (!bitwise) {
    auto fpdenormal = s.getFn().getFnAttrs().getFPDenormal(from_ty).input;
    fp_a = handle_subnormal(s, fpdenormal, std::move(fp_a));
    fp_b = handle_subnormal(s, fpdenormal, std::move(fp_b));
    fp_c = handle_subnormal(s, fpdenormal, std::move(fp_c));
  }

  function<expr(const expr&)> fn_rm
    = [&](auto &rm) { return fn(fp_a, fp_b, fp_c, rm); };
  expr val = bitwise ? fn(a, b, c, {}) : round_value(s, rm, non_poison, fn_rm);

  if (fmath.flags & FastMathFlags::NNaN) {
    non_poison.add(!fp_a.isNaN());
    if (nary >= 2)
      non_poison.add(!fp_b.isNaN());
    if (nary >= 3)
      non_poison.add(!fp_c.isNaN());
    if (!flags_in_only && val.isFloat())
      non_poison.add(!val.isNaN());
  }
  if (fmath.flags & FastMathFlags::NInf) {
    non_poison.add(!fp_a.isInf());
    if (nary >= 2)
      non_poison.add(!fp_b.isInf());
    if (nary >= 3)
      non_poison.add(!fp_c.isInf());
    if (!flags_in_only && val.isFloat())
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
  if (!flags_in_only && fmath.flags & FastMathFlags::NSZ)
    val = any_fp_zero(s, std::move(val));

  if (!bitwise && val.isFloat()) {
    val = handle_subnormal(s,
                           s.getFn().getFnAttrs().getFPDenormal(from_ty).output,
                           std::move(val));
    const FloatType &ty = to_ty ? *to_ty->getAsFloatType() : fpty;
    val = ty.fromFloat(s, val, fpty, nary, a, b, c);
  }

  return { std::move(val), non_poison() };
}

static StateValue fm_poison(State &s, expr a, const expr &ap, expr b,
                            const expr &bp,
                            function<expr(const expr&, const expr&,
                                          const expr&)> fn,
                            const Type &ty, FastMathFlags fmath,
                            FpRoundingMode rm, bool bitwise,
                            bool flags_in_only = false,
                            const Type *to_ty = nullptr) {
  return fm_poison(s, std::move(a), ap, std::move(b), bp, expr(), expr(),
                   [fn](auto &a, auto &b, auto &c, auto &rm) {
                    return fn(a, b, rm);
                   }, ty, fmath, rm, bitwise, flags_in_only, to_ty, 2);
}

static StateValue fm_poison(State &s, expr a, const expr &ap,
                            function<expr(const expr&, const expr&)> fn,
                            const Type &ty, FastMathFlags fmath,
                            FpRoundingMode rm, bool bitwise,
                            bool flags_in_only = false,
                            const Type *to_ty = nullptr) {
  return fm_poison(s, std::move(a), ap, expr(), expr(), expr(), expr(),
                   [fn](auto &a, auto &b, auto &c, auto &rm) {return fn(a, rm);},
                   ty, fmath, rm, bitwise, flags_in_only, to_ty, 1);
}

StateValue FpBinOp::toSMT(State &s) const {
  function<expr(const expr&, const expr&, const expr&)> fn;
  bool bitwise = false;

  switch (op) {
  case FAdd:
    fn = [](const expr &a, const expr &b, const expr &rm) {
      return a.fadd(b, rm);
    };
    break;

  case FSub:
    fn = [](const expr &a, const expr &b, const expr &rm) {
      return a.fsub(b, rm);
    };
    break;

  case FMul:
    fn = [](const expr &a, const expr &b, const expr &rm) {
      return a.fmul(b, rm);
    };
    break;

  case FDiv:
    fn = [](const expr &a, const expr &b, const expr &rm) {
      return a.fdiv(b, rm);
    };
    break;

  case FRem:
    fn = [&](const expr &a, const expr &b, const expr &rm) {
      auto rhs = b.fabs();
      auto rem = a.fabs().frem(rhs);
      auto signbit = rem.float2BV().sign() == 1;
      auto res = expr::mkIf(signbit, rem.fadd(rhs, rm), rem)
                     .float2BV()
                     .copysign(a.float2BV())
                     .BV2float(a);
      auto invalid = a.isInf() || b == 0;
      return expr::mkIf(invalid, expr::mkNaN(a), expr::mkIf(b.isInf(), a, res));
    };
    break;

  case FMin:
  case FMax:
    fn = [&](const expr &a, const expr &b, const expr &rm) {
      return fmin_fmax(s, a, b, rm, op == FMin);
    };
    break;

  case FMinimum:
  case FMaximum:
    fn = [&](const expr &a, const expr &b, const expr &rm) {
      return fminimum_fmaximum(s, a, b, rm, op == FMinimum);
    };
    break;

  case FMinimumnum:
  case FMaximumnum:
    fn = [&](const expr &a, const expr &b, const expr &rm) {
      expr zpos = expr::mkNumber("0", a), zneg = expr::mkNumber("-0", a);
      expr cmp = op == FMinimumnum ? a.fole(b) : a.foge(b);
      expr neg_cond = op == FMinimumnum ? (a.isFPNegative() || b.isFPNegative())
                                        : (a.isFPNegative() && b.isFPNegative());
      expr e = expr::mkIf(a.isFPZero() && b.isFPZero(),
                          expr::mkIf(neg_cond, zneg, zpos),
                          expr::mkIf(cmp, a, b));

      return expr::mkIf(a.isNaN(), b, expr::mkIf(b.isNaN(), a, e));
    };
    break;

  case CopySign:
    bitwise = true;
    fn = [](const expr &a, const expr &b, const expr &rm) {
      return a.copysign(b);
    };
    break;
  }

  auto scalar = [&](const auto &a, const auto &b, const Type &ty) {
    return fm_poison(s, a.value, a.non_poison, b.value, b.non_poison, fn,
                     ty, fmath, rm, bitwise);
  };

  auto &a = s[*lhs];
  auto &b = s[*rhs];

  if (lhs->getType().isVectorType()) {
    auto retty = getType().getAsAggregateType();
    vector<StateValue> vals;
    for (unsigned i = 0, e = retty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(retty->extract(a, i), retty->extract(b, i),
                               retty->getChild(i)));
    }
    return retty->aggregateVals(vals);
  }
  return scalar(a, b, getType());
}

expr FpBinOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceFloatOrVectorType() &&
         getType() == lhs->getType() &&
         getType() == rhs->getType();
}

unique_ptr<Instr> FpBinOp::dup(Function &f, const string &suffix) const {
  return make_unique<FpBinOp>(getType(), getName()+suffix, *lhs, *rhs, op,
                              fmath, rm, ex);
}


vector<Value*> UnaryOp::operands() const {
  return { val };
}

bool UnaryOp::propagatesPoison() const {
  switch (op) {
  case Copy:
  case BitReverse:
  case BSwap:
  case Ctpop:
  case FFS:
    return true;
  case IsConstant: return false;
  }
  UNREACHABLE();
}

bool UnaryOp::hasSideEffects() const {
  return false;
}

void UnaryOp::rauw(const Value &what, Value &with) {
  RAUW(val);

  if (auto *agg = dynamic_cast<AggregateValue*>(val))
    agg->rauw(what, with);
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
      return { std::move(one), true };

    // may or may not be a constant
    return { s.getFreshNondetVar("is.const", one), true };
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

  return Value::getTypeConstraints() && std::move(instrconstr);
}

static Value* dup_aggregate(Function &f, Value *val) {
  if (auto *agg = dynamic_cast<AggregateValue*>(val)) {
    vector<Value*> elems;
    for (auto v : agg->getVals()) {
      elems.emplace_back(dup_aggregate(f, v));
    }
    auto agg_new
      = make_unique<AggregateValue>(agg->getType(), std::move(elems));
    auto ret = agg_new.get();
    f.addAggregate(std::move(agg_new));
    return ret;
  }
  return val;
}

unique_ptr<Instr> UnaryOp::dup(Function &f, const string &suffix) const {
  auto *newval = val;
  if (dynamic_cast<AggregateValue*>(val) != nullptr && op == Copy)
    newval = dup_aggregate(f, val);

  return make_unique<UnaryOp>(getType(), getName() + suffix, *newval, op);
}


vector<Value*> FpUnaryOp::operands() const {
  return { val };
}

bool FpUnaryOp::propagatesPoison() const {
  return true;
}

bool FpUnaryOp::hasSideEffects() const {
  return false;
}

void FpUnaryOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void FpUnaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FAbs:         str = "fabs "; break;
  case FNeg:         str = "fneg "; break;
  case Canonicalize: str = "canonicalize "; break;
  case Ceil:         str = "ceil "; break;
  case Floor:        str = "floor "; break;
  case RInt:         str = "rint "; break;
  case NearbyInt:    str = "nearbyint "; break;
  case Round:        str = "round "; break;
  case RoundEven:    str = "roundeven "; break;
  case Trunc:        str = "trunc "; break;
  case Sqrt:         str = "sqrt "; break;
  }

  os << getName() << " = " << str << fmath << *val;
  if (!rm.isDefault())
    os << ", rounding=" << rm;
  if (!ex.ignore())
    os << ", exceptions=" << ex;
}

StateValue FpUnaryOp::toSMT(State &s) const {
  expr (*fn)(const expr&, const expr&) = nullptr;
  bool bitwise = false;

  switch (op) {
  case FAbs:
    bitwise = true;
    fn = [](const expr &v, const expr &rm) { return v.fabs(); };
    break;
  case FNeg:
    bitwise = true;
    fn = [](const expr &v, const expr &rm) { return v.fneg(); };
    break;
  case Canonicalize:
    fn = [](const expr &v, const expr &rm) { return v; };
    break;
  case Ceil:
    fn = [](const expr &v, const expr &rm) { return v.ceil(); };
    break;
  case Floor:
    fn = [](const expr &v, const expr &rm) { return v.floor(); };
    break;
  case RInt:
  case NearbyInt:
    // TODO: they differ in exception behavior
    fn = [](const expr &v, const expr &rm) { return v.round(rm); };
    break;
  case Round:
    fn = [](const expr &v, const expr &rm) { return v.round(expr::rna()); };
    break;
  case RoundEven:
    fn = [](const expr &v, const expr &rm) { return v.round(expr::rne()); };
    break;
  case Trunc:
    fn = [](const expr &v, const expr &rm) { return v.round(expr::rtz()); };
    break;
  case Sqrt:
    fn = [](const expr &v, const expr &rm) { return v.sqrt(rm); };
    break;
  }

  auto scalar = [&](const StateValue &v, const Type &ty) {
    return
      fm_poison(s, v.value, v.non_poison, fn, ty, fmath, rm, bitwise, false);
  };

  auto &v = s[*val];

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = val->getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(v, i), ty->getChild(i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return scalar(v, getType());
}

expr FpUnaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val->getType() &&
         getType().enforceFloatOrVectorType();
}

unique_ptr<Instr> FpUnaryOp::dup(Function &f, const string &suffix) const {
  return make_unique<FpUnaryOp>(getType(), getName() + suffix, *val, op, fmath,
                                rm, ex);
}


vector<Value*> FpUnaryOpVerticalZip::operands() const {
  return { val };
}

bool FpUnaryOpVerticalZip::propagatesPoison() const {
  return true;
}

bool FpUnaryOpVerticalZip::hasSideEffects() const {
  return false;
}

void FpUnaryOpVerticalZip::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void FpUnaryOpVerticalZip::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FrExp: str = "frexp "; break;
  }

  os << getName() << " = " << str << *val;
}

StateValue FpUnaryOpVerticalZip::toSMT(State &s) const {
  function<pair<expr, expr>(const expr&)> fn;

  switch (op) {
  case FrExp:
    fn = [&](const expr &v) {
      auto frexp = v.frexp();
      expr cond = v.isNaN() || v.isInf();
      expr nondet = s.getFreshNondetVar("frexp", frexp.second);
      return make_pair(expr::mkIf(cond, v, frexp.first),
                       expr::mkIf(cond, nondet, frexp.second));
    };
    break;
  }

  auto &v = s[*val];
  vector<StateValue> vals;

  auto scalar = [&](const StateValue &v, const Type &ty) {
    expr val2;
    auto fn2 = [&](auto &v, auto &rm){
      auto [v1, v2] = fn(v);
      val2 = std::move(v2);
      return v1;
    };
    auto v1 = fm_poison(s, v.value, v.non_poison, fn2, ty, {}, {}, false);
    return make_pair(std::move(v1),
                     StateValue(std::move(val2), expr(v.non_poison) ));
  };

  if (val->getType().isVectorType()) {
    vector<StateValue> v1s, v2s;
    auto ty = val->getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      auto [v1, v2] = scalar(ty->extract(v, i), ty->getChild(i));
      v1s.emplace_back(std::move(v1));
      v2s.emplace_back(std::move(v2));
    }
    auto retty = getType().getAsAggregateType();
    unsigned v2idx = 1 + retty->isPadding(1);
    vals.emplace_back(
      retty->getChild(0).getAsAggregateType()->aggregateVals(v1s));
    vals.emplace_back(
      retty->getChild(v2idx).getAsAggregateType()->aggregateVals(v2s));
  } else {
    auto [v1, v2] = scalar(v, val->getType());
    vals.emplace_back(std::move(v1));
    vals.emplace_back(std::move(v2));
  }
  return getType().getAsAggregateType()->aggregateVals(vals);
}

expr FpUnaryOpVerticalZip::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints() &&
           val->getType().enforceFloatOrVectorType() &&
           getType().enforceStructType();
  if (auto ty = getType().getAsStructType()) {
    unsigned v2idx = 1 + ty->isPadding(1);
    c &= ty->numElementsExcludingPadding() == 2 &&
         ty->getChild(0) == val->getType() &&
         ty->getChild(v2idx).enforceIntOrVectorType(32);
  }
  return c;
}

unique_ptr<Instr>
FpUnaryOpVerticalZip::dup(Function &f, const string &suffix) const {
  return
    make_unique<FpUnaryOpVerticalZip>(getType(), getName() + suffix, *val, op);
}


vector<Value*> UnaryReductionOp::operands() const {
  return { val };
}

bool UnaryReductionOp::propagatesPoison() const {
  return true;
}

bool UnaryReductionOp::hasSideEffects() const {
  return false;
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
      res = std::move(ith);
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
    }
    // The result is non-poisonous if all lanes are non-poisonous.
    res.non_poison &= ith.non_poison;
  }
  return res;
}

expr UnaryReductionOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         val->getType().enforceVectorType(
           [this](auto &scalar) { return scalar == getType(); });
}

unique_ptr<Instr>
UnaryReductionOp::dup(Function &f, const string &suffix) const {
  return make_unique<UnaryReductionOp>(getType(), getName() + suffix, *val, op);
}

vector<Value *> FpUnaryReductionOp::operands() const {
  return {val};
}

bool FpUnaryReductionOp::propagatesPoison() const {
  return true;
}

bool FpUnaryReductionOp::hasSideEffects() const {
  return false;
}

void FpUnaryReductionOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void FpUnaryReductionOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FMin: str = "reduce_fmin "; break;
  case FMax: str = "reduce_fmax "; break;
  case FMinimum: str = "reduce_fminimum "; break;
  case FMaximum: str = "reduce_fmaximum "; break;
  }

  os << getName() << " = " << str << print_type(val->getType())
     << val->getName();
}

StateValue FpUnaryReductionOp::toSMT(State &s) const {
  auto &v = s[*val];
  auto vty = val->getType().getAsAggregateType();
  bool bitwise = false;
  StateValue res;

  function<expr(const expr &, const expr &, const expr &)> fn;

  switch (op) {
  case FMin:
  case FMax:
    fn = [&](const expr &a, const expr &b, const expr &rm) {
      return fmin_fmax(s, a, b, rm, op == FMin);
    };
    break;
  case FMinimum:
  case FMaximum:
    fn = [&](const expr &a, const expr &b, const expr &rm) {
      return fminimum_fmaximum(s, a, b, rm, op == FMinimum);
    };
    break;
  default:
    UNREACHABLE();
  }

  auto scalar = [&](const auto &a, const auto &b, const Type &ty) {
    return fm_poison(s, a.value, a.non_poison, b.value, b.non_poison, fn, ty,
                     fmath, rm, bitwise);
  };

  for (unsigned i = 0, e = vty->numElementsConst(); i != e; ++i) {
    auto ith = vty->extract(v, i);
    if (i == 0) {
      res = std::move(ith);
      continue;
    }

    res = scalar(res, ith, getType());
  }
  return res;
}

expr FpUnaryReductionOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
    getType().enforceFloatType() &&
    val->getType().enforceVectorType(
        [this](auto &scalar) { return scalar == getType(); });
}

unique_ptr<Instr>
FpUnaryReductionOp::dup(Function &f, const string &suffix) const {
  return make_unique<FpUnaryReductionOp>(getType(), getName() + suffix, *val, op, fmath, rm, ex);
}

vector<Value*> TernaryOp::operands() const {
  return { a, b, c };
}

bool TernaryOp::propagatesPoison() const {
  return true;
}

bool TernaryOp::hasSideEffects() const {
  return false;
}

void TernaryOp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
  RAUW(c);
}

void TernaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FShl:       str = "fshl "; break;
  case FShr:       str = "fshr "; break;
  case SMulFix:    str = "smul_fix "; break;
  case UMulFix:    str = "umul_fix "; break;
  case SMulFixSat: str = "smul_fix_sat "; break;
  case UMulFixSat: str = "umul_fix_sat "; break;
  case ObjectSize: str = "objectsize "; break;
  }

  os << getName() << " = " << str << *a << ", " << *b << ", " << *c;
}

StateValue TernaryOp::toSMT(State &s) const {
  auto &av = s[*a];
  auto &bv = s[*b];
  auto &cv = s[*c];

  auto scalar = [&](const auto &a, const auto &b, const auto &c) -> StateValue {
    StateValue v(expr(), a.non_poison && b.non_poison && c.non_poison);
    switch (op) {
    case FShl:
      v.value = expr::fshl(a.value, b.value, c.value);
      break;
    case FShr:
      v.value = expr::fshr(a.value, b.value, c.value);
      break;
    case SMulFix:
      v.value       = expr::smul_fix(a.value, b.value, c.value);
      v.non_poison &= expr::smul_fix_no_soverflow(a.value, b.value, c.value);
      break;
    case UMulFix:
      v.value       = expr::umul_fix(a.value, b.value, c.value);
      v.non_poison &= expr::umul_fix_no_uoverflow(a.value, b.value, c.value);
      break;
    case SMulFixSat:
      v.value = expr::smul_fix_sat(a.value, b.value, c.value);
      break;
    case UMulFixSat:
      v.value = expr::umul_fix_sat(a.value, b.value, c.value);
      break;
    case ObjectSize: {
      Pointer ptr(s.getMemory(), a.value);
      expr ty = getType().getDummyValue(false).value;
      expr realval = ptr.leftoverSize().zextOrTrunc(ty.bits());
      v.value = s.getFreshNondetVar("objectsize", ty);
      s.addPre(expr::mkIf(b.value == 0 || (ptr.isNull() && c.value == 1),
                          v.value.uge(realval),
                          v.value.ule(realval)));
      break;
    }
    }
    return v;
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(av, i), ty->extract(bv, i),
                               (op == FShl || op == FShr) ?
                               ty->extract(cv, i) : cv));
    }
    return ty->aggregateVals(vals);
  }
  return scalar(av, bv, cv);
}

expr TernaryOp::getTypeConstraints(const Function &f) const {
  expr instrconstr;
  switch (op) {
  case FShl:
  case FShr:
    instrconstr =
      getType() == a->getType() &&
      getType() == b->getType() &&
      getType() == c->getType() &&
      getType().enforceIntOrVectorType();
    break;
  case SMulFix:
  case UMulFix:
  case SMulFixSat:
  case UMulFixSat:
    // LangRef only says that the third argument has to be an integer,
    // but the IR verifier seems to reject anything other than i32, so
    // we'll keep things simple and go with that constraint here too
    instrconstr =
      getType() == a->getType() &&
      getType() == b->getType() &&
      c->getType().enforceIntType(32) &&
      getType().enforceIntOrVectorType();
    break;
  case ObjectSize:
    instrconstr =
      getType().enforceIntType() &&
      a->getType().enforcePtrType() &&
      b->getType().enforceIntType(1) &&
      c->getType().enforceIntType(1);
    break;
  }
  return Value::getTypeConstraints() && instrconstr;
}

unique_ptr<Instr> TernaryOp::dup(Function &f, const string &suffix) const {
  return make_unique<TernaryOp>(getType(), getName() + suffix, *a, *b, *c, op);
}


vector<Value*> FpTernaryOp::operands() const {
  return { a, b, c };
}

bool FpTernaryOp::propagatesPoison() const {
  return true;
}

bool FpTernaryOp::hasSideEffects() const {
  return false;
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

  os << getName() << " = " << str << fmath << *a << ", " << *b << ", " << *c;
  if (!rm.isDefault())
    os << ", rounding=" << rm;
  if (!ex.ignore())
    os << ", exceptions=" << ex;
}

StateValue FpTernaryOp::toSMT(State &s) const {
  function<expr(const expr&, const expr&, const expr&, const expr&)> fn;

  switch (op) {
  case FMA:
    fn = [](const expr &a, const expr &b, const expr &c, const expr &rm) {
      return expr::fma(a, b, c, rm);
    };
    break;
  case MulAdd:
    fn = [&](const expr &a, const expr &b, const expr &c, const expr &rm) {
      expr var = s.getFreshNondetVar("nondet", expr(false));
      return expr::mkIf(var, expr::fma(a, b, c, rm), a.fmul(b, rm).fadd(c, rm));
    };
    break;
  }

  auto scalar = [&](const StateValue &a, const StateValue &b,
                    const StateValue &c, const Type &ty) {
    return fm_poison(s, a.value, a.non_poison, b.value, b.non_poison,
                     c.value, c.non_poison, fn, ty, fmath, rm, false);
  };

  auto &av = s[*a];
  auto &bv = s[*b];
  auto &cv = s[*c];

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(av, i), ty->extract(bv, i),
                               ty->extract(cv, i), ty->getChild(i)));
    }
    return ty->aggregateVals(vals);
  }
  return scalar(av, bv, cv, getType());
}

expr FpTernaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == a->getType() &&
         getType() == b->getType() &&
         getType() == c->getType() &&
         getType().enforceFloatOrVectorType();
}

unique_ptr<Instr> FpTernaryOp::dup(Function &f, const string &suffix) const {
  return make_unique<FpTernaryOp>(getType(), getName() + suffix, *a, *b, *c, op,
                                  fmath, rm, ex);
}


vector<Value*> TestOp::operands() const {
  return { lhs, rhs };
}

bool TestOp::propagatesPoison() const {
  return true;
}

bool TestOp::hasSideEffects() const {
  return false;
}

void TestOp::rauw(const Value &what, Value &with) {
  RAUW(lhs);
  RAUW(rhs);
}

void TestOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case Is_FPClass: str = "is.fpclass "; break;
  }

  os << getName() << " = " << str << *lhs << ", " << *rhs;
}

StateValue TestOp::toSMT(State &s) const {
  auto &a = s[*lhs];
  auto &b = s[*rhs];
  function<expr(const expr&, const Type&)> fn;

  switch (op) {
  case Is_FPClass:
    fn = [&](const expr &v, const Type &ty) -> expr {
      uint64_t mask;
      ENSURE(b.value.isUInt(mask) && b.non_poison.isTrue());
      return isfpclass(v, ty, mask).toBVBool();
    };
    break;
  }

  auto scalar = [&](const StateValue &v, const Type &ty) -> StateValue {
    return { fn(v.value, ty), expr(v.non_poison) };
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = lhs->getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(a, i), ty->getChild(i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return scalar(a, lhs->getType());
}

expr TestOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         lhs->getType().enforceFloatOrVectorType() &&
         rhs->getType().enforceIntType(32) &&
         getType().enforceIntOrVectorType(1) &&
         getType().enforceVectorTypeEquiv(lhs->getType());
}

unique_ptr<Instr> TestOp::dup(Function &f, const string &suffix) const {
  return make_unique<TestOp>(getType(), getName() + suffix, *lhs, *rhs, op);
}

ConversionOp::ConversionOp(Type &type, std::string &&name, Value &val, Op op,
                           unsigned flags)
    : Instr(type, std::move(name)), val(&val), op(op), flags(flags) {
  switch (op) {
  case ZExt:
    assert((flags & NNEG) == flags);
    break;
  case Trunc:
    assert((flags & (NSW | NUW)) == flags);
    break;
  default:
    assert(flags == 0);
    break;
  }
}

vector<Value*> ConversionOp::operands() const {
  return { val };
}

bool ConversionOp::propagatesPoison() const {
  return true;
}

bool ConversionOp::hasSideEffects() const {
  return false;
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
  case Ptr2Addr: str = "ptrtoaddr "; break;
  case Int2Ptr:  str = "inttoptr "; break;
  }

  os << getName() << " = " << str;
  if (flags & NNEG)
    os << "nneg ";
  if (flags & NSW)
    os << "nsw ";
  if (flags & NUW)
    os << "nuw ";
  os << *val << print_type(getType(), " to ", "");
}

StateValue ConversionOp::toSMT(State &s) const {
  auto v = s[*val];
  function<StateValue(expr &&, const Type &)> fn;

  switch (op) {
  case SExt:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return {val.sext(to_type.bits() - val.bits()), true};
    };
    break;
  case ZExt:
    fn = [&](auto &&val, auto &to_type) -> StateValue {
      return { val.zext(to_type.bits() - val.bits()),
               (flags & NNEG) ? !val.isNegative() : true };
    };
    break;
  case Trunc:
    fn = [this](auto &&val, auto &to_type) -> StateValue {
      AndExpr non_poison;
      unsigned orig_bits = val.bits();
      unsigned trunc_bits = to_type.bits();
      expr val_truncated = val.trunc(trunc_bits);
      if (flags & NUW)
        non_poison.add(val.extract(orig_bits-1, trunc_bits) == 0);
      if (flags & NSW)
        non_poison.add(val_truncated.sext(orig_bits - trunc_bits) == val);
      return {std::move(val_truncated), non_poison()};
    };
    break;
  case BitCast:
    // NOP: ptr vect -> ptr vect
    if (getType().isVectorType() &&
        getType().getAsAggregateType()->getChild(0).isPtrType())
      return v;

    return getType().fromInt(val->getType().toInt(s, std::move(v)));

  case Ptr2Int:
    fn = [&](auto &&val, auto &to_type) -> StateValue {
      return {s.getMemory().ptr2int(val).zextOrTrunc(to_type.bits()), true};
    };
    break;
  case Ptr2Addr:
    fn = [&](auto &&val, auto &to_type) -> StateValue {
      return
        { s.getMemory().ptr2int(val, false).zextOrTrunc(to_type.bits()), true };
    };
    break;
  case Int2Ptr:
    fn = [&](auto &&val, auto &to_type) -> StateValue {
      return {s.getMemory().int2ptr(val), true};
    };
    break;
  }

  auto scalar = [&](StateValue &&v, const Type &to_type) -> StateValue {
    auto [v2, np] = fn(std::move(v.value), to_type);
    return { std::move(v2),  v.non_poison && np };
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto retty = getType().getAsAggregateType();
    auto elems = retty->numElementsConst();
    auto valty = val->getType().getAsAggregateType();

    for (unsigned i = 0; i != elems; ++i) {
      vals.emplace_back(scalar(valty->extract(v, i), retty->getChild(i)));
    }
    return retty->aggregateVals(vals);
  }

  return scalar(std::move(v), getType());
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
  case Ptr2Addr:
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

unique_ptr<Instr> ConversionOp::dup(Function &f, const string &suffix) const {
  return
    make_unique<ConversionOp>(getType(), getName() + suffix, *val, op, flags);
}

FpConversionOp::FpConversionOp(Type &type, std::string &&name, Value &val,
                               Op op, FpRoundingMode rm, FpExceptionMode ex,
                               unsigned flags, FastMathFlags fmath)
    : Instr(type, std::move(name)), val(&val), op(op), rm(rm), ex(ex),
      flags(flags), fmath(fmath) {
  switch (op) {
  case UIntToFP:
    assert((flags & NNEG) == flags);
    break;
  default:
    assert(flags == 0);
    break;
  }
  switch (op) {
  case FPTrunc:
  case FPExt:
    break;
  default:
    assert(fmath.isNone());
    break;
  }
}

vector<Value*> FpConversionOp::operands() const {
  return { val };
}

bool FpConversionOp::propagatesPoison() const {
  return true;
}

bool FpConversionOp::hasSideEffects() const {
  return false;
}

void FpConversionOp::rauw(const Value &what, Value &with) {
  RAUW(val);
}

void FpConversionOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case SIntToFP:     str = "sitofp "; break;
  case UIntToFP:     str = "uitofp "; break;
  case FPToSInt:     str = "fptosi "; break;
  case FPToSInt_Sat: str = "fptosi_sat "; break;
  case FPToUInt:     str = "fptoui "; break;
  case FPToUInt_Sat: str = "fptoui_sat "; break;
  case FPExt:        str = "fpext "; break;
  case FPTrunc:      str = "fptrunc "; break;
  case LRInt:        str = "lrint "; break;
  case LRound:       str = "lround "; break;
  }

  os << getName() << " = " << str;
  if (flags & NNEG)
    os << "nneg ";
  if (!fmath.isNone())
    os << fmath;
  os << *val << print_type(getType(), " to ", "");
  if (!rm.isDefault())
    os << ", rounding=" << rm;
  if (!ex.ignore())
    os << ", exceptions=" << ex;
}

StateValue FpConversionOp::toSMT(State &s) const {
  auto &v = s[*val];
  function<StateValue(const expr &, const Type &, const expr &)> fn;
  function<StateValue(const StateValue &, const Type &, const Type &)> scalar;

  switch (op) {
  case SIntToFP:
    fn = [](auto &val, auto &to_type, auto &rm) -> StateValue {
      return { val.sint2fp(to_type.getAsFloatType()->getDummyFloat(), rm),
               true };
    };
    break;
  case UIntToFP:
    fn = [&](auto &val, auto &to_type, auto &rm) -> StateValue {
      return {val.uint2fp(to_type.getAsFloatType()->getDummyFloat(), rm),
              (flags & NNEG) ? !val.isNegative() : true};
    };
    break;
  case FPToSInt:
  case FPToSInt_Sat:
  case LRInt:
  case LRound:
    fn = [&](auto &val, auto &to_type, auto &rm_in) -> StateValue {
      expr rm;
      bool is_poison = false;
      switch (op) {
      case FPToSInt:
      case FPToSInt_Sat:
        rm = expr::rtz();
        is_poison = true;
        break;
      case LRInt:
        rm = rm_in;
        break;
      case LRound:
        rm = expr::rna();
        break;
      default: UNREACHABLE();
      }
      auto bits = to_type.bits();
      expr bv  = val.fp2sint(bits, rm);
      expr fp2 = bv.sint2fp(val, rm);
      // -0.xx is converted to 0 and then to 0.0, though -0.xx is ok to convert
      expr val_rounded = val.round(rm);
      expr no_overflow = val_rounded.isFPZero() || fp2 == val_rounded;

      expr np;
      if (is_poison) {
        np = std::move(no_overflow);
      } else {
        np = true;
        bv = mkIf_fold(no_overflow, bv, s.getFreshNondetVar("nondet", bv));
      }

      if (op == FPToSInt_Sat)
        return
          { expr::mkIf(val.isNaN(),
                       expr::mkUInt(0, bv),
                       expr::mkIf(np, bv, expr::mkIf(val.isFPNegative(),
                                                     expr::IntSMin(bits),
                                                     expr::IntSMax(bits)))),
            true };

      return { std::move(bv), std::move(np) };
    };
    break;
  case FPToUInt:
  case FPToUInt_Sat:
    fn = [&](auto &val, auto &to_type, auto &rm_) -> StateValue {
      auto bits = to_type.bits();
      expr rm = expr::rtz();
      expr bv  = val.fp2uint(bits, rm);
      expr fp2 = bv.uint2fp(val, rm);
      // -0.xx must be converted to 0, not poison.
      expr val_rounded = val.round(rm);
      expr no_overflow = val_rounded.isFPZero() || fp2 == val_rounded;
      if (op == FPToUInt)
        return { std::move(bv), std::move(no_overflow) };

      return { expr::mkIf(val.isNaN() || val.isFPNegative(),
                          expr::mkUInt(0, bv),
                          expr::mkIf(no_overflow, bv, expr::IntUMax(bits))),
               true };
    };
    break;
  case FPExt:
  case FPTrunc:
    scalar = [&](const StateValue &sv, const Type &from_type,
                 const Type &to_type) -> StateValue {
      auto fn = [&](auto &v, auto &rm) {
        return v.float2Float(to_type.getAsFloatType()->getDummyFloat(), rm);
      };
      return fm_poison(s, sv.value, sv.non_poison, fn, from_type, fmath, rm,
                       false, false, &to_type);
    };
    break;
  }

  if (!scalar)
    scalar = [&](const StateValue &sv, const Type &from_type,
                 const Type &to_type) -> StateValue {
      auto val = sv.value;

      if (from_type.isFloatType()) {
        auto ty = from_type.getAsFloatType();
        val = ty->getFloat(val);
      }

      function<StateValue(const expr &)> fn_rm = [&](auto &rm) {
        return fn(val, to_type, rm);
      };
      AndExpr np;
      np.add(sv.non_poison);

      StateValue ret = to_type.isFloatType() ? round_value(s, rm, np, fn_rm)
                                             : fn(val, to_type, rm.toSMT());
      np.add(std::move(ret.non_poison));

      return {to_type.isFloatType() ? to_type.getAsFloatType()->fromFloat(
                                          s, ret.value, from_type,
                                          from_type.isFloatType(), sv.value)
                                    : std::move(ret.value),
              np()};
    };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = val->getType().getAsAggregateType();
    auto retty = getType().getAsAggregateType();

    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(ty->extract(v, i), ty->getChild(i),
                               retty->getChild(i)));
    }
    return retty->aggregateVals(vals);
  }
  return scalar(v, val->getType(), getType());
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
  case FPToSInt_Sat:
  case FPToUInt:
  case FPToUInt_Sat:
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

unique_ptr<Instr> FpConversionOp::dup(Function &f, const string &suffix) const {
  return make_unique<FpConversionOp>(getType(), getName() + suffix, *val, op,
                                     rm, ex, flags, fmath);
}


vector<Value*> Select::operands() const {
  return { cond, a, b };
}

bool Select::propagatesPoison() const {
  return false;
}

bool Select::hasSideEffects() const {
  return false;
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

  auto scalar
    = [&](const auto &a, const auto &b, const auto &c, const Type &ty) {
    auto cond = c.value == 1;
    auto identity = [](const expr &x, auto &rm) { return x; };
    return fm_poison(s, expr::mkIf(cond, a.value, b.value),
                     c.non_poison &&
                       expr::mkIf(cond, a.non_poison, b.non_poison),
                     identity, ty, fmath, {}, true, /*flags_out_only=*/true);
  };

  if (auto agg = getType().getAsAggregateType()) {
    vector<StateValue> vals;
    auto cond_agg = cond->getType().getAsAggregateType();

    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (!agg->isPadding(i))
        vals.emplace_back(scalar(agg->extract(av, i), agg->extract(bv, i),
                                 cond_agg ? cond_agg->extract(cv, i) : cv,
                                 agg->getChild(i)));
    }
    return agg->aggregateVals(vals);
  }
  return scalar(av, bv, cv, getType());
}

expr Select::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         cond->getType().enforceIntOrVectorType(1) &&
         getType().enforceVectorTypeIff(cond->getType()) &&
         (fmath.isNone() ? expr(true) : getType().enforceFloatOrVectorType()) &&
         getType() == a->getType() &&
         getType() == b->getType();
}

unique_ptr<Instr> Select::dup(Function &f, const string &suffix) const {
  return
    make_unique<Select>(getType(), getName() + suffix, *cond, *a, *b, fmath);
}


void ExtractValue::addIdx(unsigned idx) {
  idxs.emplace_back(idx);
}

vector<Value*> ExtractValue::operands() const {
  return { val };
}

bool ExtractValue::propagatesPoison() const {
  return false;
}

bool ExtractValue::hasSideEffects() const {
  return false;
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

unique_ptr<Instr> ExtractValue::dup(Function &f, const string &suffix) const {
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

bool InsertValue::propagatesPoison() const {
  return false;
}

bool InsertValue::hasSideEffects() const {
  return false;
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
      vals.emplace_back(std::move(v));
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

unique_ptr<Instr> InsertValue::dup(Function &f, const string &suffix) const {
  auto ret = make_unique<InsertValue>(getType(), getName() + suffix, *val, *elt);
  for (auto idx : idxs) {
    ret->addIdx(idx);
  }
  return ret;
}


DEFINE_AS_RETZERO(FnCall, getMaxGEPOffset)

FnCall::FnCall(Type &type, string &&name, string &&fnName, FnAttrs &&attrs,
               Value *fnptr, unsigned var_arg_idx)
  : MemInstr(type, std::move(name)), fnName(std::move(fnName)), fnptr(fnptr),
    attrs(std::move(attrs)), var_arg_idx(var_arg_idx) {
  if (config::disallow_ub_exploitation)
    this->attrs.set(FnAttrs::NoUndef);
  assert(!fnptr || this->fnName.empty());
}

pair<uint64_t, uint64_t> FnCall::getMaxAllocSize() const {
  if (!hasAttribute(FnAttrs::AllocSize))
    return { 0, 1 };

  if (auto sz = getInt(*args[attrs.allocsize_0].first)) {
    if (attrs.allocsize_1 == -1u)
      return { *sz, getAlign() };

    if (auto n = getInt(*args[attrs.allocsize_1].first))
      return { mul_saturate(*sz, *n), getAlign() };
  }
  return { UINT64_MAX, getAlign() };
}

static Value* get_align_arg(const vector<pair<Value*, ParamAttrs>> &args) {
  for (auto &[arg, attrs] : args) {
    if (attrs.has(ParamAttrs::AllocAlign))
      return arg;
  }
  return nullptr;
}

Value* FnCall::getAlignArg() const {
  return get_align_arg(args);
}

uint64_t FnCall::getAlign() const {
  uint64_t align = 1;
  // TODO: add support for non constant alignments
  if (auto *arg = getAlignArg())
    align = getIntOr(*arg, 1);

  return max(align,
             attrs.has(FnAttrs::Align) ? attrs.align :
               (attrs.isAlloc() ? heap_block_alignment : 1));
}

uint64_t FnCall::getMaxAccessSize() const {
  uint64_t sz = attrs.has(FnAttrs::Dereferenceable) ? attrs.derefBytes : 0;
  if (attrs.has(FnAttrs::DereferenceableOrNull))
    sz = max(sz, attrs.derefOrNullBytes);

  for (auto &[arg, attrs] : args) {
    if (attrs.has(ParamAttrs::Dereferenceable))
      sz = max(sz, attrs.derefBytes);
    if (attrs.has(ParamAttrs::DereferenceableOrNull))
      sz = max(sz, attrs.derefOrNullBytes);
  }
  return sz;
}

MemInstr::ByteAccessInfo FnCall::getByteAccessInfo() const {
  if (attrs.has(AllocKind::Uninitialized) || attrs.has(AllocKind::Free))
    return {};

  bool has_ptr_args = any_of(args.begin(), args.end(),
    [](const auto &pair) {
      auto &[val, attrs] = pair;
      return hasPtr(val->getType()) &&
             !attrs.has(ParamAttrs::ByVal) &&
             !attrs.has(ParamAttrs::NoCapture);
    });

  // calloc style
  if (attrs.has(AllocKind::Zeroed)) {
    auto info = ByteAccessInfo::intOnly(1);
    auto [alloc, align] = getMaxAllocSize();
    if (alloc)
      info.byteSize = gcd(alloc, align);
    info.observesAddresses = has_ptr_args;
    return info;
  }

  // If bytesize is zero, this call does not participate in byte encoding.
  uint64_t bytesize = 0;

#define UPDATE(attr)                                                   \
  do {                                                                 \
    uint64_t sz = 0;                                                   \
    if (attr.has(decay<decltype(attr)>::type::Dereferenceable))        \
      sz = attr.derefBytes;                                            \
    if (attr.has(decay<decltype(attr)>::type::DereferenceableOrNull))  \
      sz = gcd(sz, attr.derefOrNullBytes);                             \
    if (sz) {                                                          \
      sz = gcd(sz, retattr.align ? retattr.align : 1);                 \
      bytesize = bytesize ? gcd(bytesize, sz) : sz;                    \
    }                                                                  \
  } while (0)

  auto &retattr = getAttributes();
  UPDATE(retattr);

  for (auto &[arg, attrs] : args) {
    if (!arg->getType().isPtrType())
      continue;

    UPDATE(attrs);
    // Pointer arguments without dereferenceable attr don't contribute to the
    // byte size.
    // call f(* dereferenceable(n) align m %p, * %q) is equivalent to a dummy
    // load followed by a function call:
    //   load i<8*n> %p, align m
    //   call f(* %p, * %q)
    // f(%p, %q) does not contribute to the bytesize. After bytesize is fixed,
    // function calls update a memory with the granularity.
  }
#undef UPDATE

  ByteAccessInfo info;
  info.byteSize = bytesize;
  info.observesAddresses = has_ptr_args;
  return info;
}


void FnCall::addArg(Value &arg, ParamAttrs &&attrs) {
  args.emplace_back(&arg, std::move(attrs));
}

vector<Value*> FnCall::operands() const {
  vector<Value*> output;
  if (fnptr)
    output.emplace_back(fnptr);
  ranges::transform(args, back_inserter(output), [](auto &p){ return p.first;});
  return output;
}

bool FnCall::propagatesPoison() const {
  return false;
}

void FnCall::rauw(const Value &what, Value &with) {
  RAUW(fnptr);
  for (auto &arg : args) {
    RAUW(arg.first);
  }
}

void FnCall::print(ostream &os) const {
  if (!isVoid())
    os << getName() << " = ";

  os << tci << "call " << print_type(getType())
     << (fnptr ? fnptr->getName() : fnName) << '(';

  bool first = true;
  unsigned idx = 0;
  for (auto &[arg, attrs] : args) {
    if (idx++ == var_arg_idx)
      os << "...";

    if (!first)
      os << ", ";

    os << attrs << *arg;
    first = false;
  }
  os << ')' << attrs;
}

static void check_can_load(State &s, const expr &p0) {
  auto &attrs = s.getFn().getFnAttrs();
  if (attrs.mem.canReadAnything())
    return;

  Pointer p(s.getMemory(), p0);
  expr readable = p.isLocal() ||
                  p.isConstGlobal() ||
                  attrs.mem.canRead(MemoryAccess::Other);
  expr nonreadable = false;

  (attrs.mem.canRead(MemoryAccess::Globals) ? readable : nonreadable)
    |= p.isWritableGlobal() && !p.isBasedOnArg();

  (attrs.mem.canRead(MemoryAccess::Args) ? readable : nonreadable)
    |= p.isBasedOnArg();

  s.addUB(std::move(readable));
  s.addUB(!nonreadable);
}

static void check_can_store(State &s, const expr &p0) {
  if (s.isInitializationPhase())
    return;

  auto &attrs = s.getFn().getFnAttrs();
  if (attrs.mem.canWriteAnything())
    return;

  Pointer p(s.getMemory(), p0);
  expr writable    = p.isLocal() || attrs.mem.canWrite(MemoryAccess::Other);
  expr nonwritable = false;

  (attrs.mem.canWrite(MemoryAccess::Globals) ? writable : nonwritable)
    |= p.isWritableGlobal() && !p.isBasedOnArg();

  (attrs.mem.canWrite(MemoryAccess::Args) ? writable : nonwritable)
    |= p.isBasedOnArg();

  s.addUB(std::move(writable));
  s.addUB(!nonwritable);
}

static void unpack_inputs(State &s, Value &argv, Type &ty,
                          const ParamAttrs &argflag, StateValue value,
                          StateValue value2, vector<StateValue> &inputs,
                          vector<PtrInput> &ptr_inputs, unsigned idx) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (agg->isPadding(i))
        continue;
      unpack_inputs(s, argv, agg->getChild(i), argflag, agg->extract(value, i),
                    agg->extract(value2, i), inputs, ptr_inputs, idx);
    }
    return;
  }

  auto unpack = [&](StateValue &&value) {
    value = argflag.encode(s, std::move(value), ty);

    if (ty.isPtrType()) {
      ptr_inputs.emplace_back(idx,
                              std::move(value),
                              expr::mkUInt(argflag.blockSize, 64),
                              argflag.has(ParamAttrs::NoRead),
                              argflag.has(ParamAttrs::NoWrite),
                              argflag.has(ParamAttrs::NoCapture));
    } else {
      inputs.emplace_back(std::move(value));
    }
  };
  unpack(std::move(value));
  unpack(std::move(value2));
}

static StateValue
check_return_value(State &s, StateValue &&val, const Type &ty,
                   const FnAttrs &attrs,
                   const vector<pair<Value*, ParamAttrs>> &args) {
  auto [allocsize, np] = attrs.computeAllocSize(s, args);
  s.addUB(std::move(np));
  return attrs.encode(s, std::move(val), ty, allocsize, get_align_arg(args));
}

static StateValue
pack_return(State &s, Type &ty, StateValue &&val, const FnAttrs &attrs,
            const vector<pair<Value*, ParamAttrs>> &args) {
  if (auto agg = ty.getAsAggregateType()) {
    vector<StateValue> vs;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (!agg->isPadding(i))
        vs.emplace_back(
          pack_return(s, agg->getChild(i), agg->extract(val, i), attrs, args));
    }
    return agg->aggregateVals(vs);
  }

  return check_return_value(s, std::move(val), ty, attrs, args);
}

StateValue FnCall::toSMT(State &s) const {
  if (approx)
    s.doesApproximation("Unknown libcall: " + fnName);

  auto &m = s.getMemory();

  vector<StateValue> inputs;
  vector<PtrInput> ptr_inputs;

  unsigned indirect_hash = 0;
  auto ptr = fnptr;
  // This is a direct call, but check if there are indirect calls elsewhere
  // to this function. If so, call it indirectly to match the other calls.
  if (!ptr && has_indirect_fncalls)
    ptr = s.getFn().getGlobalVar(fnName);

  ostringstream fnName_mangled;
  if (ptr) {
    fnName_mangled << "#indirect_call";

    Pointer p(s.getMemory(), s.getAndAddPoisonUB(*ptr, true).value);
    auto bid = p.getShortBid();
    inputs.emplace_back(expr(bid), true);
    s.addUB(p.isDereferenceable(1, 1, false));

    Function::FnDecl decl;
    decl.output = &getType();
    unsigned idx = 0;
    for (auto &[arg, params] : args) {
      if (idx++ == var_arg_idx)
        break;
      decl.inputs.emplace_back(&arg->getType(), params);
    }
    decl.is_varargs = var_arg_idx != -1u;
    s.addUB(!p.isLocal());
    s.addUB(p.getOffset() == 0);
    s.addUB(expr::mkUF("#fndeclty", { std::move(bid) }, expr::mkUInt(0, 32)) ==
            (indirect_hash = decl.hash()));
  } else {
    fnName_mangled << fnName;
  }

  StateValue ret_val;
  Type *ret_arg_ty = nullptr;
  vector<StateValue> ret_vals;
  unsigned i = 0;

  for (auto &[arg, flags] : args) {
    // we duplicate each argument so that undef values are allowed to take
    // different values so we can catch the bug in f(freeze(undef)) -> f(undef)
    StateValue sv, sv2;
    if (flags.poisonImpliesUB()) {
      sv  = s.getAndAddPoisonUB(*arg, true);
      sv2 = sv;
    } else {
      sv  = s[*arg];
      sv2 = s.eval(*arg, true);
    }

    if (ptr)
      ret_vals.emplace_back(sv);

    if (flags.has(ParamAttrs::Returned)) {
      assert(!ret_arg_ty);
      ret_val    = sv;
      ret_arg_ty = &arg->getType();
    }

    unpack_inputs(s, *arg, arg->getType(), flags, std::move(sv), std::move(sv2),
                  inputs, ptr_inputs, i);
    fnName_mangled << '#' << arg->getType();
  }
  fnName_mangled << '!' << getType();

  // Callee must return if caller must return
  if (s.getFn().has(FnAttrs::WillReturn) &&
      !attrs.has(FnAttrs::WillReturn))
    s.addGuardableUB(expr(false));

  tci.check(s, *this, ptr_inputs);

  auto get_alloc_ptr = [&]() -> Value& {
    for (auto &[arg, flags] : args) {
      if (flags.has(ParamAttrs::AllocPtr))
        return *arg;
    }
    UNREACHABLE();
  };

  if (attrs.has(AllocKind::Alloc) ||
      attrs.has(AllocKind::Realloc) ||
      attrs.has(FnAttrs::AllocSize)) {
    auto [size, np_size] = attrs.computeAllocSize(s, args);
    expr nonnull = attrs.isNonNull() ? expr(true)
                                     : expr::mkBoolVar("malloc_never_fails");
    // FIXME: alloc-family below
    // FIXME: take allocalign into account
    auto [p_new, allocated]
      = m.alloc(&size, getAlign(), Memory::MALLOC, np_size, nonnull);

    // pointer must be null if:
    // 1) alignment is not a power of 2
    // 2) size is not a multiple of alignment
    expr is_not_null = true;
    if (auto *allocalign = getAlignArg()) {
      auto &align = s[*allocalign].value;
      auto bw = max(align.bits(), size.bits());
      is_not_null &= align.isPowerOf2();
      is_not_null &= size.zextOrTrunc(bw).urem(align.zextOrTrunc(bw)) == 0;
    }

    expr nullp = Pointer::mkNullPointer(m)();
    expr ret = expr::mkIf(allocated && is_not_null, p_new, nullp);

    // TODO: In C++ we need to throw an exception if the allocation fails.

    if (attrs.has(AllocKind::Realloc)) {
      auto &[allocptr, np_ptr] = s.getAndAddUndefs(get_alloc_ptr());
      s.addUB(np_ptr);

      check_can_store(s, allocptr);

      Pointer ptr_old(m, allocptr);
      if (s.getFn().has(FnAttrs::NoFree))
        s.addGuardableUB(ptr_old.isNull() || ptr_old.isLocal());

      m.copy(ptr_old, Pointer(m, p_new));

      if (!hasAttribute(FnAttrs::NoFree)) {
        // 1) realloc(ptr, 0) always free the ptr.
        // 2) If allocation failed, we should not free previous ptr, unless it's
        // reallocf (always frees the pointer)
        expr freeptr = fnName == "@reallocf"
                        ? allocptr
                        : expr::mkIf(size == 0 || allocated, allocptr, nullp);
        m.free({std::move(freeptr), true}, false);
      }
    }

    // FIXME: for a realloc that zeroes the new stuff
    if (attrs.has(AllocKind::Zeroed))
      m.memset(p_new, { expr::mkUInt(0, 8), true }, size, getAlign(), {},
               false);

    assert(getType().isPtrType());
    return attrs.encode(s, {std::move(ret), true}, getType(), size,
                        getAlignArg());
  }
  else if (attrs.has(AllocKind::Free)) {
    auto &allocptr = s.getAndAddPoisonUB(get_alloc_ptr()).value;

    if (!hasAttribute(FnAttrs::NoFree)) {
      m.free({expr(allocptr), true}, false);

      if (s.getFn().has(FnAttrs::NoFree)) {
        Pointer ptr(m, allocptr);
        s.addGuardableUB(ptr.isNull() || ptr.isLocal());
      }
    }
    assert(isVoid());
    return {};
  }

  auto ret = s.addFnCall(std::move(fnName_mangled).str(), std::move(inputs),
                         std::move(ptr_inputs), getType(), std::move(ret_val),
                         ret_arg_ty, std::move(ret_vals), attrs, indirect_hash);

  return isVoid() ? StateValue()
                  : pack_return(s, getType(), std::move(ret), attrs, args);
}

expr FnCall::getTypeConstraints(const Function &f) const {
  // TODO : also need to name each arg type smt var uniquely
  expr ret = Value::getTypeConstraints();
  if (fnptr)
    ret &= fnptr->getType().enforcePtrType();
  return ret;
}

unique_ptr<Instr> FnCall::dup(Function &f, const string &suffix) const {
  auto r = make_unique<FnCall>(getType(), getName() + suffix, string(fnName),
                               FnAttrs(attrs), fnptr, var_arg_idx);
  r->args = args;
  r->approx = approx;
  r->tci = tci;
  return r;
}


InlineAsm::InlineAsm(Type &type, string &&name, const string &asm_str,
                     const string &constraints, FnAttrs &&attrs)
  : FnCall(type, std::move(name), "asm " + asm_str + ", " + constraints,
           std::move(attrs)) {}


ICmp::ICmp(Type &type, string &&name, Cond cond, Value &a, Value &b,
           unsigned flags)
    : Instr(type, std::move(name)), a(&a), b(&b), cond(cond), flags(flags),
      defined(cond != Any) {
  assert((flags & SameSign) == flags);
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

bool ICmp::hasSideEffects() const {
  return false;
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
  os << getName() << " = icmp ";
  if (flags & SameSign)
    os << "samesign ";
  os << condtxt << *a << ", " << b->getName();
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
  return cond == 0 ? e : build_icmp_chain(var, fn, cond, std::move(e));
}

StateValue ICmp::toSMT(State &s) const {
  auto &a_eval = s[*a];
  auto &b_eval = s[*b];

  function<expr(const expr&, const expr&, Cond)> fn =
      [](auto &av, auto &bv, Cond cond) {
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
      auto &m = s.getMemory();
      Pointer lhs(m, av);
      Pointer rhs(m, bv);

      switch (pcmode) {
      case INTEGRAL:
        m.observesAddr(lhs, false);
        m.observesAddr(rhs, false);
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
    expr np = true;
    if (flags & SameSign) {
      if (isPtrCmp()) {
        auto &m = s.getMemory();
        Pointer lhs(m, a.value);
        Pointer rhs(m, b.value);
        m.observesAddr(lhs, false);
        m.observesAddr(rhs, false);
        np = lhs.getAddress().sign() == rhs.getAddress().sign();
      } else {
        np = a.value.sign() == b.value.sign();
      }
    }
    return { v.toBVBool(), a.non_poison && b.non_poison && np };
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

unique_ptr<Instr> ICmp::dup(Function &f, const string &suffix) const {
  return make_unique<ICmp>(getType(), getName() + suffix, cond, *a, *b, flags);
}


vector<Value*> FCmp::operands() const {
  return { a, b };
}

bool FCmp::propagatesPoison() const {
  return true;
}

bool FCmp::hasSideEffects() const {
  return false;
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
  case TRUE:  condtxt = "true "; break;
  case FALSE: condtxt = "false "; break;
  }
  os << getName() << " = fcmp " << fmath << condtxt << *a << ", "
     << b->getName();
  if (signaling)
    os << ", signaling";
  if (!ex.ignore())
    os << ", exceptions=" << ex;
}

StateValue FCmp::toSMT(State &s) const {
  auto &a_eval = s[*a];
  auto &b_eval = s[*b];

  auto fn = [&](const auto &a, const auto &b, const Type &ty) -> StateValue {
    auto cmp = [&](const expr &a, const expr &b, auto &rm) {
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
      case TRUE:  return expr(true);
      case FALSE: return expr(false);
      }
    };
    auto [val, np] = fm_poison(s, a.value, a.non_poison, b.value, b.non_poison,
                               cmp, ty, fmath, {}, false, true);
    return { val.toBVBool(), std::move(np) };
  };

  if (auto agg = a->getType().getAsAggregateType()) {
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      vals.emplace_back(fn(agg->extract(a_eval, i), agg->extract(b_eval, i),
                           agg->getChild(i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return fn(a_eval, b_eval, a->getType());
}

expr FCmp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntOrVectorType(1) &&
         getType().enforceVectorTypeEquiv(a->getType()) &&
         a->getType().enforceFloatOrVectorType() &&
         a->getType() == b->getType();
}

unique_ptr<Instr> FCmp::dup(Function &f, const string &suffix) const {
  return make_unique<FCmp>(getType(), getName() + suffix, cond, *a, *b, fmath,
                           ex, signaling);
}


vector<Value*> Freeze::operands() const {
  return { val };
}

bool Freeze::propagatesPoison() const {
  return false;
}

bool Freeze::hasSideEffects() const {
  return false;
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
  return s.freeze(getType(), v);
}

expr Freeze::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val->getType();
}

unique_ptr<Instr> Freeze::dup(Function &f, const string &suffix) const {
  return make_unique<Freeze>(getType(), getName() + suffix, *val);
}


void Phi::addValue(Value &val, string &&BB_name) {
  values.emplace_back(&val, std::move(BB_name));
}

void Phi::removeValue(const string &BB_name) {
  for (auto I = values.begin(); I != values.end(); ) {
    if (I->second == BB_name) {
      I = values.erase(I);
    } else {
      ++I;
    }
  }
}

void Phi::removeValue(const Value *value) {
  for (auto I = values.begin(); I != values.end(); ) {
    if (I->first == value) {
      I = values.erase(I);
    } else {
      ++I;
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

void Phi::setValue(size_t index, Value &val) {
  values[index].first = &val;
}

void Phi::setSource(size_t index, string &&BB_name) {
  values[index].second = std::move(BB_name);
}

vector<Value*> Phi::operands() const {
  vector<Value*> v;
  for (auto &[val, bb] : values) {
    v.emplace_back(val);
  }
  return v;
}

bool Phi::propagatesPoison() const {
  return false;
}

bool Phi::hasSideEffects() const {
  return false;
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
  os << getName() << " = phi " << fmath << print_type(getType());

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

  StateValue sv = *std::move(ret)();
  auto identity = [](const expr &x, auto &rm) { return x; };
  return fm_poison(s, sv.value, sv.non_poison, identity, getType(), fmath, {},
                   true, /*flags_out_only=*/true);
}

expr Phi::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints();
  for (auto &[val, bb] : values) {
    c &= val->getType() == getType();
  }
  return c;
}

unique_ptr<Instr> Phi::dup(Function &f, const string &suffix) const {
  auto phi = make_unique<Phi>(getType(), getName() + suffix);
  for (auto &[val, bb] : values) {
    phi->addValue(*val, string(bb));
  }
  return phi;
}


bool JumpInstr::propagatesPoison() const {
  return false;
}

bool JumpInstr::hasSideEffects() const {
  return true;
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

bool JumpInstr::isTerminator() const {
  return true;
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

unique_ptr<Instr> Branch::dup(Function &f, const string &suffix) const {
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
    s.addJump(std::move(cmp), *bb);
  }

  s.addJump(std::move(default_cond), *default_target, true);
  return {};
}

expr Switch::getTypeConstraints(const Function &f) const {
  expr typ = value->getType().enforceIntType();
  for (auto &p : targets) {
    typ &= p.first->getType() == value->getType();
  }
  return typ;
}

unique_ptr<Instr> Switch::dup(Function &f, const string &suffix) const {
  auto sw = make_unique<Switch>(*value, *default_target);
  for (auto &[value_cond, bb] : targets) {
    sw->addTarget(*value_cond, *bb);
  }
  return sw;
}


vector<Value*> Return::operands() const {
  return { val };
}

bool Return::propagatesPoison() const {
  return false;
}

bool Return::hasSideEffects() const {
  return true;
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
check_ret_attributes(State &s, StateValue &&sv, const StateValue &returned_arg,
                     const Type &t, const FnAttrs &attrs,
                     const vector<pair<Value*, ParamAttrs>> &args) {
  if (auto agg = t.getAsAggregateType()) {
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (agg->isPadding(i))
        continue;
      vals.emplace_back(check_ret_attributes(s, agg->extract(sv, i),
                                             agg->extract(returned_arg, i),
                                             agg->getChild(i), attrs, args));
    }
    return agg->aggregateVals(vals);
  }

  if (t.isPtrType()) {
    Pointer p(s.getMemory(), sv.value);
    sv.non_poison &= !p.isNocapture();
  }

  auto newsv = check_return_value(s, std::move(sv), t, attrs, args);
  if (returned_arg.isValid())
    newsv.non_poison &= newsv.value == returned_arg.value;
  return newsv;
}

StateValue Return::toSMT(State &s) const {
  auto &attrs = s.getFn().getFnAttrs();
  StateValue retval = s.getMaybeUB(*val, attrs.poisonImpliesUB());

  s.addGuardableUB(s.getMemory().returnChecks());

  // Overwrite any dead_on_return pointee block with poison upon return.
  auto &m = s.getMemory();
  const auto &inputs = s.getFn().getInputs();
  StateValue poison = {expr::mkUInt(0, 8), false};
  for (auto &arg : inputs) {
    if (!arg.getType().isPtrType())
      continue;
    auto &attrs = static_cast<const Input&>(arg).getAttributes();
    if (attrs.has(ParamAttrs::DeadOnReturn))
      m.memset(s[arg].value, poison, {}, bits_byte / 8, {}, false, true);
  }

  vector<pair<Value*, ParamAttrs>> args;
  for (auto &arg : inputs) {
    args.emplace_back(const_cast<Value*>(&arg), ParamAttrs());
  }

  if (attrs.has(FnAttrs::NoReturn))
    s.addGuardableUB(expr(false));

  s.addReturn(check_ret_attributes(s, std::move(retval),
                                   s.getReturnedInput().value_or(StateValue()),
                                   getType(), attrs, args));
  return {};
}

expr Return::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == val->getType() &&
         f.getType() == getType();
}

unique_ptr<Instr> Return::dup(Function &f, const string &suffix) const {
  return make_unique<Return>(getType(), *val);
}

bool Return::isTerminator() const {
  return true;
}

Assume::Assume(Value &cond, Kind kind)
    : MemInstr(Type::voidTy, "assume"), args({&cond}), kind(kind) {
  assert(kind == AndNonPoison || kind == WellDefined || kind == NonNull);
}

Assume::Assume(vector<Value *> &&args0, Kind kind)
    : MemInstr(Type::voidTy, "assume"), args(std::move(args0)), kind(kind) {
  switch (kind) {
    case AndNonPoison:
    case WellDefined:
    case NonNull:
      assert(args.size() == 1);
      break;

    case Align:
    case Dereferenceable:
    case DereferenceableOrNull:
      assert(args.size() == 2);
      break;
  }
}

vector<Value*> Assume::operands() const {
  return args;
}

bool Assume::propagatesPoison() const {
  return false;
}

bool Assume::hasSideEffects() const {
  switch (kind) {
  // assume(true) is NOP
  case AndNonPoison:
  case WellDefined:
    if (auto *c = dynamic_cast<IntConst*>(args[0]))
      if (auto n = c->getInt())
        return *n != 1;
    break;
  default:
    break;
  }
  return true;
}

DEFINE_AS_RETZERO(Assume, getMaxGEPOffset)
DEFINE_AS_RETZEROALIGN(Assume, getMaxAllocSize)
DEFINE_AS_EMPTYACCESS(Assume)

uint64_t Assume::getMaxAccessSize() const {
  switch (kind) {
  case AndNonPoison:
  case WellDefined:
  case NonNull:
    return 0;
  case Align:
  case Dereferenceable:
  case DereferenceableOrNull:
    return getIntOr(*args[1], UINT64_MAX);
  }
  UNREACHABLE();
}

void Assume::rauw(const Value &what, Value &with) {
  for (auto &arg: args)
    RAUW(arg);
}

void Assume::print(ostream &os) const {
  const char *str = nullptr;
  switch (kind) {
  case AndNonPoison:          str = "assume "; break;
  case WellDefined:           str = "assume_welldefined "; break;
  case Align:                 str = "assume_align "; break;
  case Dereferenceable:       str = "assume_dereferenceable "; break;
  case DereferenceableOrNull: str = "assume_dereferenceable_or_null "; break;
  case NonNull:               str = "assume_nonnull "; break;
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
    if (config::disallow_ub_exploitation && v.value.isZero())
      s.addUnreachable();
    s.addGuardableUB(v.value != 0);
    break;
  }
  case WellDefined:
    (void)s.getAndAddPoisonUB(*args[0], true);
    break;
  case Align: {
    // assume(ptr, align)
    const auto &ptr   = s.getAndAddPoisonUB(*args[0]).value;
    const auto &align = s.getAndAddPoisonUB(*args[1]).value;
    s.addGuardableUB(Pointer(s.getMemory(), ptr).isAligned(align));
    break;
  }
  case Dereferenceable:
  case DereferenceableOrNull: {
    const auto &vptr  = s.getAndAddPoisonUB(*args[0]).value;
    const auto &bytes = s.getAndAddPoisonUB(*args[1]).value;
    Pointer ptr(s.getMemory(), vptr);
    expr nonnull = kind == DereferenceableOrNull ? !ptr.isNull() : false;
    s.addUB(nonnull || merge(ptr.isDereferenceable(bytes, 1, false)));
    break;
  }
  case NonNull: {
    // assume(ptr)
    const auto &ptr = s.getAndAddPoisonUB(*args[0]).value;
    s.addGuardableUB(!Pointer(s.getMemory(), ptr).isNull());
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
    return args[0]->getType().enforceIntType();
  case Align:
  case Dereferenceable:
  case DereferenceableOrNull:
    return args[0]->getType().enforcePtrType() &&
           args[1]->getType().enforceIntType();
  case NonNull:
    return args[0]->getType().enforcePtrType();
  }
  UNREACHABLE();
}

unique_ptr<Instr> Assume::dup(Function &f, const string &suffix) const {
  return make_unique<Assume>(vector<Value *>(args), kind);
}


AssumeVal::AssumeVal(Type &type, string &&name, Value &val,
                     vector<Value *> &&args0, Kind kind, bool is_welldefined)
    : MemInstr(type, std::move(name)), val(&val), args(std::move(args0)),
      kind(kind), is_welldefined(is_welldefined) {
  switch (kind) {
  case Align:
    assert(args.size() == 1);
    break;
  case NonNull:
    assert(args.empty());
    break;
  case Range:
    assert((args.size() & 1) == 0);
    break;
  }
}

vector<Value*> AssumeVal::operands() const {
  auto ret = args;
  ret.emplace_back(val);
  return ret;
}

bool AssumeVal::propagatesPoison() const {
  return true;
}

bool AssumeVal::hasSideEffects() const {
  return false;
}

DEFINE_AS_RETZERO(AssumeVal, getMaxGEPOffset)
DEFINE_AS_RETZEROALIGN(AssumeVal, getMaxAllocSize)
DEFINE_AS_EMPTYACCESS(AssumeVal)

uint64_t AssumeVal::getMaxAccessSize() const {
  switch (kind) {
  case NonNull:
  case Range:
    return 0;
  case Align:
    return getIntOr(*args[0], UINT64_MAX);
  }
  UNREACHABLE();
}

void AssumeVal::rauw(const Value &what, Value &with) {
  RAUW(val);
  for (auto &arg: args)
    RAUW(arg);
}

void AssumeVal::print(ostream &os) const {
  const char *str = nullptr;
  switch (kind) {
  case Align:   str = "align "; break;
  case NonNull: str = "nonnull "; break;
  case Range:   str = "range "; break;
  }

  os << getName() << " = !" << str << *val;

  for (auto &arg: args) {
    os << ", " << *arg;
  }

  if (is_welldefined)
    os << ", welldefined";
}

StateValue AssumeVal::toSMT(State &s) const {
  function<expr(const expr&)> fn;

  switch (kind) {
  case Align:
    fn = [&](const expr &v) {
      uint64_t n;
      ENSURE(s[*args[0]].value.isUInt(n));
      return Pointer(s.getMemory(), expr(v)).isAligned(n);
    };
    break;

  case NonNull:
    fn = [&](const expr &v) {
      return !Pointer(s.getMemory(), expr(v)).isNull();
    };
    break;

  case Range: // val in [l1, h1) U ... U [ln, hn) (signed)
    fn = [&](const expr &v) {
      OrExpr inrange;
      for (unsigned i = 0, e = args.size(); i != e; i += 2) {
        auto &lb = s[*args[i]].value;
        auto &hb = s[*args[i+1]].value;
        auto l = v.sge(lb);
        auto h = v.slt(hb);

        if (lb.sgt(hb).isTrue()) { // wrapping interval
          inrange.add(std::move(l));
          inrange.add(std::move(h));
        } else {
          inrange.add(l && h);
        }
      }
      return std::move(inrange)();
    };
    break;
  }

  auto scalar = [&](const Type &ty, const StateValue &v) -> StateValue {
    expr np = fn(v.value);

    if (config::disallow_ub_exploitation)
      s.addGuardableUB(expr(np));

    if (is_welldefined) {
      s.addUB(std::move(np));
      np = true;
    }

    StateValue res(expr(v.value), v.non_poison && np);
    // there's no poison in assembly
    if (s.isAsmMode()) {
      res = s.freeze(ty, res);
    }
    return res;
  };

  auto &v = s.getMaybeUB(*val, is_welldefined);
  if (auto agg = getType().getAsAggregateType()) {
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(agg->getChild(i), agg->extract(v, i)));
    }
    return getType().getAsAggregateType()->aggregateVals(vals);
  }
  return scalar(getType(), v);
}

expr AssumeVal::getTypeConstraints(const Function &f) const {
  expr e = true;
  switch (kind) {
  case Align:
    e = args[0]->getType().isIntType();
    break;
  case NonNull:
    break;
  case Range: {
    e = getType().enforceIntOrVectorType();
    for (auto &arg : args) {
      e &= getType().enforceScalarOrVectorType(
                       [&](auto &ty) { return ty == arg->getType(); });
    }
    break;
  }
  }
  return getType() == val->getType() && e;
}

unique_ptr<Instr> AssumeVal::dup(Function &f, const string &suffix) const {
  return make_unique<AssumeVal>(getType(), getName() + suffix, *val,
                                vector<Value*>(args), kind, is_welldefined);
}


bool MemInstr::hasSideEffects() const {
  return true;
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
  info.subByteAccess    = t.maxSubBitAccess();
  return info;
}

MemInstr::ByteAccessInfo MemInstr::ByteAccessInfo::full(unsigned byteSize) {
  return { true, true, true, true, byteSize, 0 };
}


DEFINE_AS_RETZERO(Alloc, getMaxAccessSize)
DEFINE_AS_RETZERO(Alloc, getMaxGEPOffset)
DEFINE_AS_EMPTYACCESS(Alloc)

pair<uint64_t, uint64_t> Alloc::getMaxAllocSize() const {
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

bool Alloc::propagatesPoison() const {
  return true;
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
      s.addGuardableUB(mul_e == 0 || sz.extract(sz.bits()-1, bits_size_t) == 0);
    sz = sz.zextOrTrunc(bits_size_t);

    if (mul_e.bits() > bits_size_t)
      s.addGuardableUB(mul_e.extract(mul_e.bits()-1, bits_size_t) == 0);
    auto m = mul_e.zextOrTrunc(bits_size_t);

    s.addGuardableUB(sz.mul_no_uoverflow(m));
    sz = sz * m;
  }

  expr ptr = s.getMemory().alloc(&sz, align, Memory::STACK, true, true).first;
  if (initially_dead)
    s.getMemory().free({expr(ptr), true}, true);
  return { std::move(ptr), true };
}

expr Alloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size->getType().enforceIntType();
}

unique_ptr<Instr> Alloc::dup(Function &f, const string &suffix) const {
  auto a = make_unique<Alloc>(getType(), getName() + suffix, *size, mul, align);
  if (initially_dead)
    a->markAsInitiallyDead();
  return a;
}


DEFINE_AS_RETZEROALIGN(StartLifetime, getMaxAllocSize)
DEFINE_AS_RETZERO(StartLifetime, getMaxAccessSize)
DEFINE_AS_RETZERO(StartLifetime, getMaxGEPOffset)
DEFINE_AS_EMPTYACCESS(StartLifetime)

vector<Value*> StartLifetime::operands() const {
  return { ptr };
}

bool StartLifetime::propagatesPoison() const {
  return false;
}

void StartLifetime::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void StartLifetime::print(ostream &os) const {
  os << "start_lifetime " << *ptr;
}

StateValue StartLifetime::toSMT(State &s) const {
  s.getMemory().startLifetime(s[*ptr]);
  return {};
}

expr StartLifetime::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> StartLifetime::dup(Function &f, const string &suffix) const {
  return make_unique<StartLifetime>(*ptr);
}


DEFINE_AS_RETZEROALIGN(EndLifetime, getMaxAllocSize)
DEFINE_AS_RETZERO(EndLifetime, getMaxAccessSize)
DEFINE_AS_RETZERO(EndLifetime, getMaxGEPOffset)
DEFINE_AS_EMPTYACCESS(EndLifetime)

vector<Value*> EndLifetime::operands() const {
  return { ptr };
}

bool EndLifetime::propagatesPoison() const {
  return false;
}

void EndLifetime::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void EndLifetime::print(ostream &os) const {
  os << "end_lifetime " << *ptr;
}

StateValue EndLifetime::toSMT(State &s) const {
  auto &pv = s[*ptr];
  Pointer p(s.getMemory(), pv.value);
  s.addUB(pv.non_poison.implies(p.isStackAllocated()));
  s.getMemory().free(pv, true);
  return {};
}

expr EndLifetime::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> EndLifetime::dup(Function &f, const string &suffix) const {
  return make_unique<EndLifetime>(*ptr);
}


void GEP::addIdx(uint64_t obj_size, Value &idx) {
  idxs.emplace_back(obj_size, &idx);
}

DEFINE_AS_RETZEROALIGN(GEP, getMaxAllocSize)
DEFINE_AS_RETZERO(GEP, getMaxAccessSize)
DEFINE_AS_EMPTYACCESS(GEP)

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
      off = add_saturate(off, (int64_t)mul * *n);
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

bool GEP::propagatesPoison() const {
  return true;
}

bool GEP::hasSideEffects() const {
  return false;
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
  else if (nusw)
    os << "nusw ";
  if (nuw)
    os << "nuw ";
  os << *ptr;

  for (auto &[sz, idx] : idxs) {
    os << ", " << sz << " x " << *idx;
  }
}

StateValue GEP::toSMT(State &s) const {
  auto scalar = [&](const StateValue &ptrval,
                    vector<pair<uint64_t, StateValue>> &offsets) -> StateValue {
    Pointer ptr(s.getMemory(), ptrval.value);
    AndExpr non_poison(ptrval.non_poison);
    AndExpr inbounds_np;
    AndExpr idx_all_zeros;

    if (inbounds) {
      // FIXME: not implemented for physical pointers
      s.addUB(ptr.isLogical());
      s.doesApproximation("gep inbounds of phy ptr", !ptr.isLogical(), true);
      inbounds_np.add(ptr.inbounds(false, true));
    }

    expr offset_sum = expr::mkUInt(0, bits_for_offset);
    for (auto &[sz, idx] : offsets) {
      auto &[v, np] = idx;
      auto multiplier = expr::mkUInt(sz, offset_sum);
      auto val = v.sextOrTrunc(bits_for_offset);
      auto inc = multiplier * val;

      if (inbounds && sz != 0)
        idx_all_zeros.add(v == 0);

      if (nusw) {
        if (v.bits() > bits_program_pointer)
          non_poison.add(
            v.trunc(bits_program_pointer).sextOrTrunc(v.bits()) == v);
        non_poison.add(multiplier.mul_no_soverflow(val));
        non_poison.add(ptr.addNoUSOverflow(inc, inbounds));
        if (!inbounds) {
          // For non-inbounds gep, we have to explicitly check that adding the
          // offsets without the base address also doesn't wrap.
          non_poison.add(offset_sum.add_no_soverflow(inc));
          offset_sum = offset_sum + inc;
        }
      }

      if (nuw) {
        if (v.bits() > bits_program_pointer)
          non_poison.add(v.extract(v.bits()-1, bits_program_pointer) == 0);
        non_poison.add(multiplier.mul_no_uoverflow(val));
        non_poison.add(ptr.addNoUOverflow(inc, inbounds));
      }

#ifndef NDEBUG
      int64_t n;
      if (inc.isInt(n))
        assert(ilog2_ceil(abs(n), true) <= bits_for_offset);
#endif

      ptr += inc;
      non_poison.add(np);

      if (inbounds)
        inbounds_np.add(ptr.inbounds(false, true));
    }

    if (inbounds) {
      auto all_zeros = idx_all_zeros();
      auto inbounds_cond = all_zeros || inbounds_np();

      if (config::disallow_ub_exploitation)
        s.addGuardableUB(non_poison().implies(inbounds_cond));
      else
        non_poison.add(std::move(inbounds_cond));

      // try to simplify the pointer
      if (all_zeros.isFalse())
        ptr.inbounds(true, true);
    }

    return { std::move(ptr).release(), non_poison() };
  };

  if (auto aty = getType().getAsAggregateType()) {
    vector<StateValue> vals;
    auto &ptrval = s[*ptr];
    bool ptr_isvect = ptr->getType().isVectorType();

    for (unsigned i = 0, e = aty->numElementsConst(); i != e; ++i) {
      vector<pair<uint64_t, StateValue>> offsets;
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
  vector<pair<uint64_t, StateValue>> offsets;
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

unique_ptr<Instr> GEP::dup(Function &f, const string &suffix) const {
  auto dup = make_unique<GEP>(getType(), getName() + suffix, *ptr, inbounds,
                              nusw, nuw);
  for (auto &[sz, idx] : idxs) {
    dup->addIdx(sz, *idx);
  }
  return dup;
}


DEFINE_AS_RETZEROALIGN(PtrMask, getMaxAllocSize)
DEFINE_AS_RETZERO(PtrMask, getMaxAccessSize)
DEFINE_AS_EMPTYACCESS(PtrMask)

uint64_t PtrMask::getMaxGEPOffset() const {
  if (auto n = getInt(*mask))
    return ~*n;
  return UINT64_MAX;
}

optional<uint64_t> PtrMask::getExactAlign() const {
  if (auto n = getInt(*mask)) {
    uint64_t align = -*n;
    if (is_power2(align))
      return align;
  }
  return {};
}

vector<Value*> PtrMask::operands() const {
  return { ptr, mask };
}

bool PtrMask::propagatesPoison() const {
  return true;
}

bool PtrMask::hasSideEffects() const {
  return false;
}

void PtrMask::rauw(const Value &what, Value &with) {
  RAUW(ptr);
  RAUW(mask);
}

void PtrMask::print(ostream &os) const {
  os << getName() << " = ptrmask " << *ptr << ", " << *mask;
}

StateValue PtrMask::toSMT(State &s) const {
  auto &ptrval  = s[*ptr];
  auto &maskval = s[*mask];

  auto fn = [&](const StateValue &ptrval, const StateValue &mask) -> StateValue {
    Pointer ptr(s.getMemory(), ptrval.value);
    return { ptr.maskOffset(mask.value).release(),
             ptrval.non_poison && mask.non_poison };
  };

  if (auto agg = getType().getAsAggregateType()) {
    auto maskTy = mask->getType().getAsAggregateType();
    assert(maskTy);
    vector<StateValue> vals;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      vals.emplace_back(fn(agg->extract(ptrval, i),
                           maskTy->extract(maskval, i)));
    }
    return agg->aggregateVals(vals);
  }
  return fn(ptrval, maskval);
}

expr PtrMask::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr->getType().enforcePtrOrVectorType() &&
         getType() == ptr->getType() &&
         mask->getType().enforceIntOrVectorType() &&
         ptr->getType().enforceVectorTypeIff(mask->getType());
}

unique_ptr<Instr> PtrMask::dup(Function &f, const string &suffix) const {
  return make_unique<PtrMask>(getType(), getName() + suffix, *ptr, *mask);
}


DEFINE_AS_RETZEROALIGN(Load, getMaxAllocSize)
DEFINE_AS_RETZERO(Load, getMaxGEPOffset)

uint64_t Load::getMaxAccessSize() const {
  return Memory::getStoreByteSize(getType());
}

MemInstr::ByteAccessInfo Load::getByteAccessInfo() const {
  return ByteAccessInfo::get(getType(), false, align);
}

vector<Value*> Load::operands() const {
  return { ptr };
}

bool Load::propagatesPoison() const {
  return true;
}

void Load::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Load::print(ostream &os) const {
  os << getName() << " = load " << getType() << ", " << *ptr
     << ", align " << align;
}

StateValue Load::toSMT(State &s) const {
  auto &p = s.getWellDefinedPtr(*ptr);
  check_can_load(s, p);
  auto [sv, ub] = s.getMemory().load(p, getType(), align);
  s.addUB(std::move(ub));
  return sv;
}

expr Load::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Load::dup(Function &f, const string &suffix) const {
  return make_unique<Load>(getType(), getName() + suffix, *ptr, align);
}


DEFINE_AS_RETZEROALIGN(Store, getMaxAllocSize)
DEFINE_AS_RETZERO(Store, getMaxGEPOffset)

uint64_t Store::getMaxAccessSize() const {
  return Memory::getStoreByteSize(val->getType());
}

MemInstr::ByteAccessInfo Store::getByteAccessInfo() const {
  return ByteAccessInfo::get(val->getType(), true, align);
}

vector<Value*> Store::operands() const {
  return { val, ptr };
}

bool Store::propagatesPoison() const {
  return false;
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

  auto &p = s.getWellDefinedPtr(*ptr);
  check_can_store(s, p);
  auto &v = s[*val];
  s.getMemory().store(p, v, val->getType(), align, s.getUndefVars());
  return {};
}

expr Store::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Store::dup(Function &f, const string &suffix) const {
  return make_unique<Store>(*ptr, *val, align);
}


DEFINE_AS_RETZEROALIGN(Memset, getMaxAllocSize)
DEFINE_AS_RETZERO(Memset, getMaxGEPOffset)

uint64_t Memset::getMaxAccessSize() const {
  return getIntOr(*bytes, UINT64_MAX);
}

MemInstr::ByteAccessInfo Memset::getByteAccessInfo() const {
  unsigned byteSize = 1;
  if (auto bs = getInt(*bytes))
    byteSize = gcd(align, *bs);
  return ByteAccessInfo::intOnly(byteSize);
}

vector<Value*> Memset::operands() const {
  return { ptr, val, bytes };
}

bool Memset::propagatesPoison() const {
  return false;
}

void Memset::rauw(const Value &what, Value &with) {
  RAUW(ptr);
  RAUW(val);
  RAUW(bytes);
}

void Memset::print(ostream &os) const {
  os << tci << "memset " << *ptr << " align " << align << ", " << *val << ", "
     << *bytes;
}

StateValue Memset::toSMT(State &s) const {
  auto &vbytes = s.getAndAddPoisonUB(*bytes, true).value;

  uint64_t n;
  expr vptr;
  if (vbytes.isUInt(n) && n > 0) {
    vptr = s.getWellDefinedPtr(*ptr);
  } else {
    auto &sv_ptr = s[*ptr];
    auto &sv_ptr2 = s[*ptr];
    // can't be poison even if bytes=0 as address must be aligned regardless
    s.addGuardableUB(expr(sv_ptr.non_poison));
    // disallow undef ptrs
    s.addGuardableUB((vbytes != 0).implies(sv_ptr.value == sv_ptr2.value));
    vptr = sv_ptr.value;
  }
  check_can_store(s, vptr);
  tci.check(s, *this, { vptr });

  s.getMemory().memset(vptr, s[*val].zextOrTrunc(8), vbytes, align,
                       s.getUndefVars());
  return {};
}

expr Memset::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType() &&
         val->getType().enforceIntType() &&
         bytes->getType().enforceIntType();
}

unique_ptr<Instr> Memset::dup(Function &f, const string &suffix) const {
  return make_unique<Memset>(*ptr, *val, *bytes, align, tci);
}


DEFINE_AS_RETZEROALIGN(MemsetPattern, getMaxAllocSize)
DEFINE_AS_RETZERO(MemsetPattern, getMaxGEPOffset)

MemsetPattern::MemsetPattern(Value &ptr, Value &pattern, Value &bytes,
                             unsigned pattern_length, TailCallInfo tci)
  : MemInstr(Type::voidTy, "memset_pattern" + to_string(pattern_length)),
    ptr(&ptr), pattern(&pattern), bytes(&bytes),
    pattern_length(pattern_length), tci(tci) {}

uint64_t MemsetPattern::getMaxAccessSize() const {
  return getIntOr(*bytes, UINT64_MAX);
}

MemInstr::ByteAccessInfo MemsetPattern::getByteAccessInfo() const {
  unsigned byteSize = 1;
  if (auto bs = getInt(*bytes))
    byteSize = *bs;
  return ByteAccessInfo::intOnly(byteSize);
}

vector<Value*> MemsetPattern::operands() const {
  return { ptr, pattern, bytes };
}

bool MemsetPattern::propagatesPoison() const {
  return false;
}

void MemsetPattern::rauw(const Value &what, Value &with) {
  RAUW(ptr);
  RAUW(pattern);
  RAUW(bytes);
}

void MemsetPattern::print(ostream &os) const {
  os << getName() << ' ' << tci << *ptr << ", " << *pattern << ", " << *bytes;
}

StateValue MemsetPattern::toSMT(State &s) const {
  auto &vptr = s.getWellDefinedPtr(*ptr);
  auto &vpattern = s.getAndAddPoisonUB(*pattern, false).value;
  auto &vbytes = s.getAndAddPoisonUB(*bytes, true).value;
  check_can_store(s, vptr);
  check_can_load(s, vpattern);
  tci.check(s, *this, { vptr });

  s.getMemory().memset_pattern(vptr, vpattern, vbytes, pattern_length);
  return {};
}

expr MemsetPattern::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType() &&
         pattern->getType().enforcePtrType() &&
         bytes->getType().enforceIntType();
}

unique_ptr<Instr> MemsetPattern::dup(Function &f, const string &suffix) const {
  return make_unique<MemsetPattern>(*ptr, *pattern, *bytes, pattern_length, tci);
}


DEFINE_AS_RETZEROALIGN(FillPoison, getMaxAllocSize)
DEFINE_AS_RETZERO(FillPoison, getMaxGEPOffset)

uint64_t FillPoison::getMaxAccessSize() const {
  return getGlobalVarSize(ptr);
}

MemInstr::ByteAccessInfo FillPoison::getByteAccessInfo() const {
  return ByteAccessInfo::intOnly(1);
}

vector<Value*> FillPoison::operands() const {
  return { ptr };
}

bool FillPoison::propagatesPoison() const {
  return true;
}

void FillPoison::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void FillPoison::print(ostream &os) const {
  os << "fillpoison " << *ptr;
}

StateValue FillPoison::toSMT(State &s) const {
  auto &vptr = s.getWellDefinedPtr(*ptr);
  Memory &m = s.getMemory();
  m.fillPoison(Pointer(m, vptr).getBid());
  return {};
}

expr FillPoison::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> FillPoison::dup(Function &f, const string &suffix) const {
  return make_unique<FillPoison>(*ptr);
}


DEFINE_AS_RETZEROALIGN(Memcpy, getMaxAllocSize)
DEFINE_AS_RETZERO(Memcpy, getMaxGEPOffset)

uint64_t Memcpy::getMaxAccessSize() const {
  return getIntOr(*bytes, UINT64_MAX);
}

MemInstr::ByteAccessInfo Memcpy::getByteAccessInfo() const {
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

bool Memcpy::propagatesPoison() const {
  return false;
}

void Memcpy::rauw(const Value &what, Value &with) {
  RAUW(dst);
  RAUW(src);
  RAUW(bytes);
}

void Memcpy::print(ostream &os) const {
  os << tci << (move ? "memmove " : "memcpy ") << *dst << " align " << align_dst
     << ", " << *src << " align " << align_src << ", " << *bytes;
}

StateValue Memcpy::toSMT(State &s) const {
  auto &vbytes = s.getAndAddPoisonUB(*bytes, true).value;

  uint64_t n;
  expr vsrc, vdst;
  if (align_dst || (vbytes.isUInt(n) && n > 0)) {
    vdst = s.getWellDefinedPtr(*dst);
  } else {
    auto &sv_dst = s[*dst];
    auto &sv_dst2 = s[*dst];
    s.addGuardableUB((vbytes != 0).implies(sv_dst.implies(sv_dst2)));
    vdst = sv_dst.value;
  }

  if (align_src || (vbytes.isUInt(n) && n > 0)) {
    vsrc = s.getWellDefinedPtr(*src);
  } else {
    auto &sv_src = s[*src];
    auto &sv_src2 = s[*src];
    s.addGuardableUB((vbytes != 0).implies(sv_src.implies(sv_src2)));
    vsrc = sv_src.value;
  }

  if (vbytes.bits() > bits_size_t)
    s.addUB(
      vbytes.ule(expr::IntUMax(bits_size_t).zext(vbytes.bits() - bits_size_t)));

  check_can_load(s, vsrc);
  check_can_store(s, vdst);
  tci.check(s, *this, { vsrc, vdst });

  s.getMemory().memcpy(vdst, vsrc, vbytes, align_dst, align_src, move);
  return {};
}

expr Memcpy::getTypeConstraints(const Function &f) const {
  return dst->getType().enforcePtrType() &&
         dst->getType().enforcePtrType() &&
         bytes->getType().enforceIntType();
}

unique_ptr<Instr> Memcpy::dup(Function &f, const string &suffix) const {
  return
    make_unique<Memcpy>(*dst, *src, *bytes, align_dst, align_src, move, tci);
}



DEFINE_AS_RETZEROALIGN(Memcmp, getMaxAllocSize)
DEFINE_AS_RETZERO(Memcmp, getMaxGEPOffset)

uint64_t Memcmp::getMaxAccessSize() const {
  return getIntOr(*num, UINT64_MAX);
}

MemInstr::ByteAccessInfo Memcmp::getByteAccessInfo() const {
  auto info = ByteAccessInfo::anyType(1);
  info.observesAddresses = true;
  return info;
}

vector<Value*> Memcmp::operands() const {
  return { ptr1, ptr2, num };
}

bool Memcmp::propagatesPoison() const {
  return false;
}

void Memcmp::rauw(const Value &what, Value &with) {
  RAUW(ptr1);
  RAUW(ptr2);
  RAUW(num);
}

void Memcmp::print(ostream &os) const {
  os << getName() << " = " << tci << (is_bcmp ? "bcmp " : "memcmp ") << *ptr1
     << ", " << *ptr2 << ", " << *num;
}

StateValue Memcmp::toSMT(State &s) const {
  auto &stptr1 = s[*ptr1];
  auto &stptr2 = s[*ptr2];
  auto &[vptr1, np1] = stptr1;
  auto &[vptr2, np2] = stptr2;
  auto &vnum = s.getAndAddPoisonUB(*num).value;
  s.addGuardableUB((vnum != 0).implies(np1 && np2));

  check_can_load(s, vptr1);
  check_can_load(s, vptr2);
  tci.check(s, *this, { stptr1, stptr2 });

  Pointer p1(s.getMemory(), vptr1), p2(s.getMemory(), vptr2);
  // memcmp can be optimized to load & icmps, and it requires this
  // dereferenceability check of vnum.
  s.addUB(p1.isDereferenceable(vnum, 1, false));
  s.addUB(p2.isDereferenceable(vnum, 1, false));

  expr zero = expr::mkUInt(0, 32);

  expr result_var, result_var_neg;
  if (is_bcmp) {
    result_var = s.getFreshNondetVar("bcmp_nonzero", zero);
    s.addPre(result_var != zero);
  } else {
    auto z31 = expr::mkUInt(0, 31);
    result_var = s.getFreshNondetVar("memcmp_nonzero", z31);
    s.addPre(result_var != z31);
    result_var = expr::mkUInt(0, 1).concat(result_var);

    result_var_neg = s.getFreshNondetVar("memcmp", z31);
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
      expr pos = val1.forceCastToInt().uge(val2.forceCastToInt());
      result_neq = expr::mkIf(pos, result_var, result_var_neg);
    }

    expr val_eq = val1.forceCastToInt() == val2.forceCastToInt();

    // allow null <-> 0 comparison
    expr np
      = (is_ptr1 == is_ptr2 || val1.isZero() || val2.isZero()) &&
        !val1.isPoison() && !val2.isPoison();

    return { expr::mkIf(val_eq, zero, result_neq),
             std::move(np), {},
             val_eq && vnum.uge(i + 2) };
  };
  auto [val, np, ub]
    = LoopLikeFunctionApproximator(ith_exec).encode(s, memcmp_unroll_cnt);
  return { expr::mkIf(vnum == 0, zero, std::move(val)), (vnum != 0).implies(np) };
}

expr Memcmp::getTypeConstraints(const Function &f) const {
  return ptr1->getType().enforcePtrType() &&
         ptr2->getType().enforcePtrType() &&
         num->getType().enforceIntType();
}

unique_ptr<Instr> Memcmp::dup(Function &f, const string &suffix) const {
  return make_unique<Memcmp>(getType(), getName() + suffix, *ptr1, *ptr2, *num,
                             is_bcmp, tci);
}


DEFINE_AS_RETZEROALIGN(Strlen, getMaxAllocSize)
DEFINE_AS_RETZERO(Strlen, getMaxGEPOffset)

uint64_t Strlen::getMaxAccessSize() const {
  return getGlobalVarSize(ptr);
}

MemInstr::ByteAccessInfo Strlen::getByteAccessInfo() const {
  return ByteAccessInfo::intOnly(1); /* strlen raises UB on ptr bytes */
}

vector<Value*> Strlen::operands() const {
  return { ptr };
}

bool Strlen::propagatesPoison() const {
  return true;
}

void Strlen::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Strlen::print(ostream &os) const {
  os << getName() << " = " << tci << "strlen " << *ptr;
}

StateValue Strlen::toSMT(State &s) const {
  auto &eptr = s.getWellDefinedPtr(*ptr);
  check_can_load(s, eptr);
  tci.check(s, *this, { eptr });

  Pointer p(s.getMemory(), eptr);
  Type &ty = getType();

  auto ith_exec =
      [&s, &p, &ty](unsigned i, bool _) -> tuple<expr, expr, AndExpr, expr> {
    AndExpr ub;
    auto [val, ub_load] = s.getMemory().load((p + i)(), IntType("i8", 8), 1);
    ub.add(std::move(ub_load.first));
    ub.add(std::move(ub_load.second));
    ub.add(std::move(val.non_poison));
    return { expr::mkUInt(i, ty.bits()), true, std::move(ub), val.value != 0 };
  };
  auto [val, _, ub]
    = LoopLikeFunctionApproximator(ith_exec).encode(s, strlen_unroll_cnt);
  s.addUB(std::move(ub));
  return { std::move(val), true };
}

expr Strlen::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr->getType().enforcePtrType() &&
         getType().enforceIntType();
}

unique_ptr<Instr> Strlen::dup(Function &f, const string &suffix) const {
  return make_unique<Strlen>(getType(), getName() + suffix, *ptr, tci);
}


vector<Value*> VaStart::operands() const {
  return { ptr };
}

bool VaStart::propagatesPoison() const {
  return true;
}

bool VaStart::hasSideEffects() const {
  return true;
}

void VaStart::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void VaStart::print(ostream &os) const {
  os << "call void @llvm.va_start(" << *ptr << ')';
}

StateValue VaStart::toSMT(State &s) const {
  s.addGuardableUB(expr(s.getFn().isVarArgs()));

  auto &data  = s.getVarArgsData();
  auto &raw_p = s.getWellDefinedPtr(*ptr);

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
    matched_one.add(std::move(eq));
  }

  Pointer ptr(s.getMemory(), raw_p);
  s.addUB(ptr.isBlockAlive());
  s.addUB(ptr.blockSize().uge(4)); // FIXME: this is target dependent

  // alive, next_arg, num_args, is_va_start, active
  data.try_emplace(raw_p, expr(true), std::move(zero), std::move(num_args),
                   expr(true), !matched_one());

  return {};
}

expr VaStart::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> VaStart::dup(Function &f, const string &suffix) const {
  return make_unique<VaStart>(*ptr);
}


vector<Value*> VaEnd::operands() const {
  return { ptr };
}

bool VaEnd::propagatesPoison() const {
  return true;
}

bool VaEnd::hasSideEffects() const {
  return true;
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
  auto &raw_p = s.getWellDefinedPtr(*ptr);

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

unique_ptr<Instr> VaEnd::dup(Function &f, const string &suffix) const {
  return make_unique<VaEnd>(*ptr);
}


vector<Value*> VaCopy::operands() const {
  return { dst, src };
}

bool VaCopy::propagatesPoison() const {
  return true;
}

bool VaCopy::hasSideEffects() const {
  return true;
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
  auto &dst_raw = s.getWellDefinedPtr(*dst);
  auto &src_raw = s.getWellDefinedPtr(*src);
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
    is_va_start.add(entry.is_va_start, std::move(select));

    // kill aliases
    entry.active &= ptr != dst_raw;
  }

  // FIXME: dst should be empty or we have a mem leak
  // alive, next_arg, num_args, is_va_start, active
  data[dst_raw] = { true, *std::move(next_arg)(), *std::move(num_args)(),
                    *std::move(is_va_start)(), true };

  return {};
}

expr VaCopy::getTypeConstraints(const Function &f) const {
  return dst->getType().enforcePtrType() &&
         src->getType().enforcePtrType();
}

unique_ptr<Instr> VaCopy::dup(Function &f, const string &suffix) const {
  return make_unique<VaCopy>(*dst, *src);
}


vector<Value*> VaArg::operands() const {
  return { ptr };
}

bool VaArg::propagatesPoison() const {
  return true;
}

bool VaArg::hasSideEffects() const {
  return true;
}

void VaArg::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void VaArg::print(ostream &os) const {
  os << getName() << " = va_arg " << *ptr << ", " << getType();
}

StateValue VaArg::toSMT(State &s) const {
  auto &data  = s.getVarArgsData();
  auto &raw_p = s.getWellDefinedPtr(*ptr);

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
    ret.add(std::move(val), select);

    expr next_arg = entry.next_arg + one;
    s.addUB(select.implies(entry.alive && entry.num_args.uge(next_arg)));
    entry.next_arg = expr::mkIf(eq, next_arg, entry.next_arg);
  }

  return *std::move(ret)();
}

expr VaArg::getTypeConstraints(const Function &f) const {
  return getType().enforceScalarType() &&
         ptr->getType().enforcePtrType();
}

unique_ptr<Instr> VaArg::dup(Function &f, const string &suffix) const {
  return make_unique<VaArg>(getType(), getName() + suffix, *ptr);
}


vector<Value*> ExtractElement::operands() const {
  return { v, idx };
}

bool ExtractElement::propagatesPoison() const {
  // the output depends on the idx, so it may not propagate to all lanes
  return false;
}

bool ExtractElement::hasSideEffects() const {
  return false;
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
  return { std::move(rv), ip && inbounds && rp };
}

expr ExtractElement::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         v->getType().enforceVectorType([&](auto &ty)
                                        { return ty == getType(); }) &&
         idx->getType().enforceIntType();
}

unique_ptr<Instr> ExtractElement::dup(Function &f, const string &suffix) const {
  return make_unique<ExtractElement>(getType(), getName() + suffix, *v, *idx);
}


vector<Value*> InsertElement::operands() const {
  return { v, e, idx };
}

bool InsertElement::propagatesPoison() const {
  return false;
}

bool InsertElement::hasSideEffects() const {
  return false;
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
  return { std::move(rv), expr::mkIf(ip && inbounds, std::move(rp),
                                vty->getDummyValue(false).non_poison) };
}

expr InsertElement::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType() == v->getType() &&
         v->getType().enforceVectorType([&](auto &ty)
                                        { return ty == e->getType(); }) &&
         idx->getType().enforceIntType();
}

unique_ptr<Instr> InsertElement::dup(Function &f, const string &suffix) const {
  return make_unique<InsertElement>(getType(), getName() + suffix,
                                    *v, *e, *idx);
}


vector<Value*> ShuffleVector::operands() const {
  return { v1, v2 };
}

bool ShuffleVector::propagatesPoison() const {
  // the output depends on the mask, so it may not propagate to all lanes
  return false;
}

bool ShuffleVector::hasSideEffects() const {
  return false;
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
      vals.emplace_back(vty->getChild(0).getDummyValue(false));
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

unique_ptr<Instr> ShuffleVector::dup(Function &f, const string &suffix) const {
  return make_unique<ShuffleVector>(getType(), getName() + suffix,
                                    *v1, *v2, mask);
}


const ConversionOp* isCast(ConversionOp::Op op, const Value &v) {
  auto c = dynamic_cast<const ConversionOp*>(&v);
  return (c && c->getOp() == op) ? c : nullptr;
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
