// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "ir/function.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <functional>
#include <iostream>

using namespace smt;
using namespace std;

#define RAUW(val)    \
  if (val == &what)  \
    val = &with

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
}


namespace IR {

expr Instr::eqType(const Instr &i) const {
  return getType() == i.getType();
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
  case FAdd:
  case FSub:
  case FMul:
  case FDiv:
  case FRem:
    assert((flags & FastMath) == flags);
    break;
  case SRem:
  case URem:
  case SAdd_Sat:
  case UAdd_Sat:
  case SSub_Sat:
  case USub_Sat:
  case And:
  case Or:
  case Xor:
  case Cttz:
  case Ctlz:
  case SAdd_Overflow:
  case UAdd_Overflow:
  case SSub_Overflow:
  case USub_Overflow:
  case SMul_Overflow:
  case UMul_Overflow:
    assert(flags == 0);
    break;
  }
}

vector<Value*> BinOp::operands() const {
  return { lhs, rhs };
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
  case FAdd:          str = "fadd "; break;
  case FSub:          str = "fsub "; break;
  case FMul:          str = "fmul "; break;
  case FDiv:          str = "fdiv "; break;
  case FRem:          str = "frem "; break;
  }

  os << getName() << " = " << str;

  if (flags & NSW)
    os << "nsw ";
  if (flags & NUW)
    os << "nuw ";
  if (flags & Exact)
    os << "exact ";
  if ((flags & FastMath) == FastMath) {
    os << "fast ";
  } else {
    if (flags & NNaN)
      os << "nnan ";
    if (flags & NInf)
      os << "ninf ";
    if (flags & NSZ)
      os << "nsz ";
    if (flags & ARCP)
      os << "arcp ";
    if (flags & Contract)
      os << "contract ";
    if (flags & Reassoc)
      os << "reassoc ";
  }
  os << print_type(getType()) << lhs->getName() << ", " << rhs->getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  auto bits = b.bits();
  s.addUB(bp);
  s.addUB(b != 0);
  if (sign)
    s.addUB((ap && a != expr::IntSMin(bits)) || b != expr::mkInt(-1, bits));
}

static expr fm_poison(const expr &a, const expr &b, const expr &val,
                      unsigned flags) {
  expr ret = false;
  if (flags & BinOp::NNaN)
    ret |= a.isNaN() || b.isNaN() || val.isNaN();
  if (flags & BinOp::NInf)
    ret |= a.isInf() || b.isInf() || val.isInf();
  if (flags & BinOp::ARCP)
    ret |= expr(); // TODO
  if (flags & BinOp::Contract)
    ret |= expr(); // TODO
  if (flags & BinOp::Reassoc)
    ret |= expr(); // TODO
  return ret;
}

StateValue BinOp::toSMT(State &s) const {
  bool vertical_zip = false;
  function<StateValue(const expr&, const expr&, const expr&, const expr&)>
    fn, scalar_op;

  auto fm_arg = [&](const expr &val) {
    if (flags & NSZ)
      return expr::mkIf(val.isFPNegZero(), expr::mkNumber("0", val), val);
    return val;
  };

  switch (op) {
  case Add:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.add_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.add_no_uoverflow(b);
      return { a + b, move(non_poison) };
    };
    break;

  case Sub:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.sub_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.sub_no_uoverflow(b);
      return { a - b, move(non_poison) };
    };
    break;

  case Mul:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      expr non_poison = true;
      if (flags & NSW)
        non_poison &= a.mul_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.mul_no_uoverflow(b);
      return { a * b, move(non_poison) };
    };
    break;

  case SDiv:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      expr non_poison = true;
      div_ub(s, a, b, ap, bp, true);
      if (flags & Exact)
        non_poison = a.sdiv_exact(b);
      return { a.sdiv(b), move(non_poison) };
    };
    break;

  case UDiv:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      expr non_poison = true;
      div_ub(s, a, b, ap, bp, false);
      if (flags & Exact)
        non_poison &= a.udiv_exact(b);
      return { a.udiv(b), move(non_poison) };
    };
    break;

  case SRem:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      div_ub(s, a, b, ap, bp, true);
      return { a.srem(b), true };
    };
    break;

  case URem:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      div_ub(s, a, b, ap, bp, false);
      return { a.urem(b), true };
    };
    break;

  case Shl:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & NSW)
        non_poison &= a.shl_no_soverflow(b);
      if (flags & NUW)
        non_poison &= a.shl_no_uoverflow(b);

      return { a << b, move(non_poison) };
    };
    break;

  case AShr:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & Exact)
        non_poison &= a.ashr_exact(b);
      return { a.ashr(b), move(non_poison) };
    };
    break;

  case LShr:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto non_poison = b.ult(b.bits());
      if (flags & Exact)
        non_poison &= a.lshr_exact(b);
      return { a.lshr(b), move(non_poison) };
    };
    break;

  case SAdd_Sat:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.sadd_sat(b), true };
    };
    break;

  case UAdd_Sat:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.uadd_sat(b), true };
    };
    break;

  case SSub_Sat:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.ssub_sat(b), true };
    };
    break;

  case USub_Sat:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.usub_sat(b), true };
    };
    break;

  case And:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a & b, true };
    };
    break;

  case Or:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a | b, true };
    };
    break;

  case Xor:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a ^ b, true };
    };
    break;

  case Cttz:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.cttz(),
               b == 0u || a != 0u };
    };
    break;

  case Ctlz:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.ctlz(),
               b == 0u || a != 0u };
    };
    break;

  case SAdd_Overflow:
    vertical_zip = true;
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a + b, (!a.add_no_soverflow(b)).toBVBool() };
    };
    break;

  case UAdd_Overflow:
    vertical_zip = true;
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a + b, (!a.add_no_uoverflow(b)).toBVBool() };
    };
    break;

  case SSub_Overflow:
    vertical_zip = true;
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a - b, (!a.sub_no_soverflow(b)).toBVBool() };
    };
    break;

  case USub_Overflow:
    vertical_zip = true;
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a - b, (!a.sub_no_uoverflow(b)).toBVBool() };
    };
    break;

  case SMul_Overflow:
    vertical_zip = true;
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a * b, (!a.mul_no_soverflow(b)).toBVBool() };
    };
    break;

  case UMul_Overflow:
    vertical_zip = true;
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a * b, (!a.mul_no_uoverflow(b)).toBVBool() };
    };
    break;

  case FAdd:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto val = fm_arg(a).fadd(fm_arg(b));
      auto non_poison = !fm_poison(a, b, val, flags);
      return { move(val), move(non_poison) };
    };
    break;

  case FSub:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto val = fm_arg(a).fsub(fm_arg(b));
      auto non_poison = !fm_poison(a, b, val, flags);
      return { move(val), move(non_poison) };
    };
    break;

  case FMul:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto val = fm_arg(a).fmul(fm_arg(b));
      auto non_poison = !fm_poison(a, b, val, flags);
      return { move(val), move(non_poison) };
    };
    break;

  case FDiv:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto val = fm_arg(a).fdiv(fm_arg(b));
      auto non_poison = !fm_poison(a, b, val, flags);
      return { move(val), move(non_poison) };
    };
    break;

  case FRem:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      // TODO; Z3 has no support for LLVM's frem which is actually an fmod
      auto val = expr();
      auto non_poison = !fm_poison(a, b, val, flags);
      return { move(val), move(non_poison) };
    };
    break;
  }

  function<pair<StateValue,StateValue>(const expr&, const expr&, const expr&,
                                       const expr&)> zip_op;
  if (vertical_zip) {
    zip_op = [&](auto a, auto ap, auto b, auto bp) {
      auto [v1, v2] = fn(a, ap, b, bp);
      expr non_poison = ap && bp;
      StateValue sv1(move(v1), expr(non_poison));
      return make_pair(move(sv1), StateValue(move(v2), move(non_poison)));
    };
  } else {
    scalar_op = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      auto [v, np] = fn(a, ap, b, bp);
      return { move(v), ap && bp && np };
    };
  }

  auto &a = s[*lhs];
  auto &b = s[*rhs];

  if (lhs->getType().isVectorType()) {
    auto ty = getType().getAsAggregateType();
    vector<StateValue> vals;

    if (vertical_zip) {
      auto ty = lhs->getType().getAsAggregateType();
      vector<StateValue> vals1, vals2;

      for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
        auto ai = ty->extract(a, i);
        auto bi = ty->extract(b, i);
        auto [v1, v2] = zip_op(ai.value, ai.non_poison, bi.value,
                               bi.non_poison);
        vals1.emplace_back(move(v1));
        vals2.emplace_back(move(v2));
      }
      vals.emplace_back(ty->aggregateVals(vals1));
      vals.emplace_back(ty->aggregateVals(vals2));
    } else {
      StateValue tmp;
      for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
        auto ai = ty->extract(a, i);
        const StateValue *bi;
        switch (op) {
        case Cttz:
        case Ctlz:
          bi = &b;
          break;
        default:
          tmp = ty->extract(b, i);
          bi = &tmp;
          break;
        }
        vals.emplace_back(scalar_op(ai.value, ai.non_poison, bi->value,
                                    bi->non_poison));
      }
    }
    return ty->aggregateVals(vals);
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
      instrconstr &= ty->numElements() == 2 &&
                     ty->getChild(0) == lhs->getType() &&
                     ty->getChild(1).enforceIntOrVectorType(1) &&
                     ty->getChild(1).enforceVectorTypeEquiv(lhs->getType());
    }
    break;
  case Cttz:
  case Ctlz:
    instrconstr = getType().enforceIntOrVectorType() &&
                  getType() == lhs->getType() &&
                  rhs->getType().enforceIntType(1);
    break;
  case FAdd:
  case FSub:
  case FMul:
  case FDiv:
  case FRem:
    instrconstr = getType().enforceFloatOrVectorType() &&
                  getType() == lhs->getType() &&
                  getType() == rhs->getType();
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


vector<Value*> UnaryOp::operands() const {
  return { val };
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
  case FNeg:        str = "fneg "; break;
  }

  os << getName() << " = " << str << print_type(getType()) << val->getName();
}

StateValue UnaryOp::toSMT(State &s) const {
  function<expr(const expr&)> fn;

  switch (op) {
  case Copy:
    fn = [](auto v) { return v; };
    break;
  case BitReverse:
    fn = [](auto v) { return v.bitreverse(); };
    break;
  case BSwap:
    fn = [](auto v) { return v.bswap(); };
    break;
  case Ctpop:
    fn = [](auto v) { return v.ctpop(); };
    break;
  case FNeg:
    fn = [](auto v) { return v.fneg(); };
    break;
  }

  auto &v = s[*val];

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      auto vi = ty->extract(v, i);
      vals.emplace_back(fn(vi.value), move(vi.non_poison));
    }
    return ty->aggregateVals(vals);
  }
  return { fn(v.value), expr(v.non_poison) };
}

expr UnaryOp::getTypeConstraints(const Function &f) const {
  expr instrconstr;
  switch(op) {
  case Copy:
    instrconstr = true;
    break;
  case BSwap:
    instrconstr = getType().enforceScalarOrVectorType([](auto &scalar) {
                    return scalar.enforceIntType() &&
                           scalar.sizeVar().urem(expr::mkUInt(16, 8)) == 0;
                  });
    break;
  case BitReverse:
  case Ctpop:
    instrconstr = getType().enforceIntOrVectorType();
    break;
  case FNeg:
    instrconstr = getType().enforceFloatOrVectorType();
    break;
  }

  return Value::getTypeConstraints() &&
         getType() == val->getType() &&
         move(instrconstr);
}

unique_ptr<Instr> UnaryOp::dup(const string &suffix) const {
  return make_unique<UnaryOp>(getType(), getName() + suffix, *val, op);
}


vector<Value*> TernaryOp::operands() const {
  return { a, b, c };
}

void TernaryOp::rauw(const Value &what, Value &with) {
  RAUW(a);
  RAUW(b);
  RAUW(c);
}

void TernaryOp::print(ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case FShl:
    str = "fshl ";
    break;
  case FShr:
    str = "fshr ";
    break;
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
    fn = [](auto a, auto b, auto c) {
      return expr::fshl(a, b, c);
    };
    break;
  case FShr:
    fn = [](auto a, auto b, auto c) {
      return expr::fshr(a, b, c);
    };
    break;
  }

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto ty = getType().getAsAggregateType();
    for (unsigned i = 0, e = ty->numElementsConst(); i != e; ++i) {
      auto ai = ty->extract(av, i);
      auto bi = ty->extract(bv, i);
      auto ci = ty->extract(cv, i);
      vals.emplace_back(fn(ai.value, bi.value, ci.value),
                        ai.non_poison && bi.non_poison && ci.non_poison);
    }
    return ty->aggregateVals(vals);
  }
  return { fn(av.value, bv.value, cv.value),
           av.non_poison && bv.non_poison && cv.non_poison };
}

expr TernaryOp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntOrVectorType() &&
         getType() == a->getType() &&
         getType() == b->getType() &&
         getType() == c->getType();
}

unique_ptr<Instr> TernaryOp::dup(const string &suffix) const {
  return make_unique<TernaryOp>(getType(), getName() + suffix, *a, *b, *c, op);
}


vector<Value*> ConversionOp::operands() const {
  return { val };
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
  case SIntToFP: str = "sitofp "; break;
  case UIntToFP: str = "uitofp "; break;
  case FPToSInt: str = "fptosi "; break;
  case FPToUInt: str = "fptoui "; break;
  case Ptr2Int:  str = "ptrtoint "; break;
  case Int2Ptr:  str = "int2ptr "; break;
  }

  os << getName() << " = " << str << *val << print_type(getType(), " to ", "");
}

StateValue ConversionOp::toSMT(State &s) const {
  auto &v = s[*val];
  function<StateValue(const expr &, const Type &, const Type &)> fn;

  switch (op) {
  case SExt:
    fn = [](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.sext(to_type.bits() - val.bits()), true };
    };
    break;
  case ZExt:
    fn = [](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.zext(to_type.bits() - val.bits()), true };
    };
    break;
  case Trunc:
    fn = [](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.trunc(to_type.bits()), true };
    };
    break;
  case BitCast:
    fn = [](auto &val, auto &from_type, auto &to_type) -> StateValue {
      expr newval;
      if ((to_type.isIntType() && from_type.isIntType()) ||
          (to_type.isFloatType() && from_type.isFloatType()) ||
          (to_type.isPtrType() && from_type.isPtrType()))
        newval = val;
      else if (to_type.isIntType() && from_type.isFloatType())
        newval = val.float2BV();
      else if (to_type.isFloatType() && from_type.isIntType())
        newval = val.BV2float(to_type.getDummyValue(true).value);
      else
        UNREACHABLE();
      return { move(newval), true };
    };
    break;
  case SIntToFP:
    fn = [](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.sint2fp(to_type.getDummyValue(false).value), true };
    };
    break;
  case UIntToFP:
    fn = [](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.uint2fp(to_type.getDummyValue(false).value), true };
    };
    break;
  case FPToSInt:
    fn = [&](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.fp2sint(to_type.bits()),
               val.foge(expr::IntSMin(getType().bits()).sint2fp(val)) &&
               val.fole(expr::IntSMax(getType().bits()).sint2fp(val)) &&
               !val.isInf() };
    };
    break;
  case FPToUInt:
    fn = [&](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { val.fp2uint(to_type.bits()),
               val.foge(expr::mkFloat(0, val)) &&
               val.fole(expr::IntUMax(getType().bits()).uint2fp(val)) &&
               !val.isInf() };
    };
    break;
  case Ptr2Int:
    fn = [&](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { s.getMemory().ptr2int(val).zextOrTrunc(to_type.bits()), true };
    };
    break;
  case Int2Ptr:
    fn = [&](auto &val, auto &from_type, auto &to_type) -> StateValue {
      return { s.getMemory().int2ptr(val), true };
    };
    break;
  }

  auto scalar = [&](const StateValue &sv, const Type &from_type,
                    const Type &to_type) -> StateValue {
    auto [v, np] = fn(sv.value, from_type, to_type);
    return { move(v), sv.non_poison && np };
  };

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto valty = val->getType().getAsAggregateType();
    auto retty = getType().getAsAggregateType();
    for (unsigned i = 0, e = valty->numElementsConst(); i != e; ++i) {
      vals.emplace_back(scalar(valty->extract(v, i), valty->getChild(i),
                               retty->getChild(i)));
    }
    return retty->aggregateVals(vals);
  }
  return scalar(v, val->getType(), getType());
}

expr ConversionOp::getTypeConstraints(const Function &f) const {
  expr c;
  switch (op) {
  case SExt:
  case ZExt:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceIntOrVectorType() &&
        val->getType().sizeVar().ult(getType().sizeVar());
    break;
  case Trunc:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceIntOrVectorType() &&
        getType().sizeVar().ult(val->getType().sizeVar());
    break;
  case BitCast:
    c = (getType().enforcePtrOrVectorType() &&
         val->getType().enforcePtrOrVectorType()) ||
        ((getType().enforceIntOrVectorType() ||
          getType().enforceFloatOrVectorType()) &&
         (val->getType().enforceIntOrVectorType() ||
          val->getType().enforceFloatOrVectorType()));

    c &= getType().sizeVar() == val->getType().sizeVar();
    break;
  case SIntToFP:
  case UIntToFP:
    c = getType().enforceFloatOrVectorType() &&
        val->getType().enforceIntOrVectorType();
    break;
  case FPToSInt:
  case FPToUInt:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceFloatOrVectorType();
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
  return Value::getTypeConstraints() &&
         move(c) &&
         getType().enforceVectorTypeEquiv(val->getType());
}

unique_ptr<Instr> ConversionOp::dup(const string &suffix) const {
  return make_unique<ConversionOp>(getType(), getName() + suffix, *val, op);
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
  os << getName() << " = select " << *cond << ", " << *a << ", " << *b;
}

StateValue Select::toSMT(State &s) const {
  auto &cv = s[*cond];
  auto &av = s[*a];
  auto &bv = s[*b];

  auto scalar = [](const auto &a, const auto &b, const auto &c) -> StateValue {
    auto cond = c.value == 1;
    return { expr::mkIf(cond, a.value, b.value),
             c.non_poison && expr::mkIf(cond, a.non_poison, b.non_poison) };
  };

  if (auto agg = getType().getAsAggregateType()) {
    vector<StateValue> vals;
    auto cond_agg = cond->getType().getAsAggregateType();

    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
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
         getType().enforceIntOrFloatOrPtrOrVectorType() &&
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
    if (++i == idxs.size())
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


void FnCall::addArg(Value &arg) {
  args.emplace_back(&arg);
}

vector<Value*> FnCall::operands() const {
  return args;
}

void FnCall::rauw(const Value &what, Value &with) {
  for (auto &arg : args) {
    RAUW(arg);
  }
}

void FnCall::print(ostream &os) const {
  if (!dynamic_cast<VoidType*>(&getType()))
    os << getName() << " = ";

  os << "call " << print_type(getType()) << fnName << '(';

  bool first = true;
  for (auto arg : args) {
    if (!first)
      os << ", ";
    os << *arg;
    first = false;
  }
  os << ')';

  if (!valid)
    os << "\t; WARNING: unknown known function";
}

StateValue FnCall::toSMT(State &s) const {
  if (!valid) {
    s.addUB({});
    return {};
  }

  // TODO: add support for memory
  // TODO: add support for global variables
  vector<expr> all_args, value_args;
  expr all_args_np(true);

  for (auto arg : args) {
    auto &[v, np] = s[*arg];
    all_args.emplace_back(v);
    all_args.emplace_back(np);
    value_args.emplace_back(v);
    all_args_np &= np;
  }

  auto add_implies_axiom = [&](const string &fn) {
    vector<expr> vars_src, vars_tgt;
    unsigned i = 0;
    for (auto &arg : all_args) {
      auto name = "v" + to_string(i++);
      vars_src.emplace_back(expr::mkVar(name.c_str(), arg));
      name = "v" + to_string(i++);
      vars_tgt.emplace_back(expr::mkVar(name.c_str(), arg));
    }

    // fn src implies fn tgt if each src arg is poison or it's equal to tgt
    set<expr> vars_set(vars_src.begin(), vars_src.end());
    vars_set.insert(vars_tgt.begin(), vars_tgt.end());

    expr cond(true);
    for (unsigned i = 0, e = vars_src.size(); i != e; i += 2) {
      cond &= !vars_src[i + 1] ||
              (vars_src[i] == vars_tgt[i] && vars_tgt[i+1]);
    }

    auto fn_src = expr::mkUF(fn.c_str(), vars_src, false);
    auto fn_tgt = expr::mkUF(fn.c_str(), vars_tgt, false);
    s.addAxiom(expr::mkForAll(vars_set, cond.implies(fn_src.implies(fn_tgt))));
  };

  // impact of the function on the domain of the program
  // TODO: constraint when certain attributes are on
  auto ub_name = fnName + "#ub";
  s.addUB(expr::mkUF(ub_name.c_str(), all_args, false));
  add_implies_axiom(ub_name);

  if (dynamic_cast<VoidType*>(&getType()))
    return {};

  // create a new variable that can take any value if function is poison
  StateValue ret_type = getType().getDummyValue(true);
  expr val = expr::mkUF(fnName.c_str(), value_args, ret_type.value);
  if (!all_args_np.isTrue()) {
    auto var_name = fnName + "_val";
    auto var = expr::mkFreshVar(var_name.c_str(), ret_type.value);
    s.addQuantVar(var);
    val = expr::mkIf(all_args_np, val, var);
  }

  // FIXME: broken for functions that return aggregate types
  if (getType().isAggregateType())
    return {};

  auto poison_name = fnName + "#poison";
  add_implies_axiom(poison_name);

  return { move(val), expr::mkUF(poison_name.c_str(), all_args, false) };
}

expr FnCall::getTypeConstraints(const Function &f) const {
  // TODO : also need to name each arg type smt var uniquely
  auto r = Value::getTypeConstraints();
  // TODO: add aggregate support
  for (auto arg : args) {
    r &= !arg->getType().enforceStructType();
    r &= !arg->getType().enforceVectorType();
  }
  return r;
}

unique_ptr<Instr> FnCall::dup(const string &suffix) const {
  auto r = make_unique<FnCall>(getType(), getName() + suffix, string(fnName),
                               valid);
  r->args = args;
  return r;
}


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
}

static StateValue build_icmp_chain(const expr &var,
                                   const function<StateValue(ICmp::Cond)> &fn,
                                   ICmp::Cond cond = ICmp::Any,
                                   StateValue last = StateValue()) {
  auto old_cond = cond;
  cond = ICmp::Cond(cond - 1);

  if (old_cond == ICmp::Any)
    return build_icmp_chain(var, fn, cond, fn(cond));

  auto e = StateValue::mkIf(var == cond, fn(cond), last);
  return cond == 0 ? e : build_icmp_chain(var, fn, cond, move(e));
}

StateValue ICmp::toSMT(State &s) const {
  auto &a_eval = s[*a];
  auto &b_eval = s[*b];
  function<StateValue(const expr&, const expr&, Cond)> fn;

  auto &elem_ty = a->getType();
  if (elem_ty.isPtrType() ||
      (elem_ty.isVectorType() &&
       elem_ty.getAsAggregateType()->getChild(0).isPtrType())) {
    fn = [&](auto &av, auto &bv, Cond cond) {
      Pointer lhs(s.getMemory(), av);
      Pointer rhs(s.getMemory(), bv);
      switch (cond) {
      case EQ:  return StateValue(lhs == rhs, true);
      case NE:  return StateValue(lhs != rhs, true);
      case SLE: return lhs.sle(rhs);
      case SLT: return lhs.slt(rhs);
      case SGE: return lhs.sge(rhs);
      case SGT: return lhs.sgt(rhs);
      case ULE: return lhs.ule(rhs);
      case ULT: return lhs.ult(rhs);
      case UGE: return lhs.uge(rhs);
      case UGT: return lhs.ugt(rhs);
      case Any:
        UNREACHABLE();
      }
      UNREACHABLE();
    };

  } else {  // integer comparison
    fn = [&](auto &av, auto &bv, Cond cond) {
      switch (cond) {
      case EQ:  return StateValue(av == bv, true);
      case NE:  return StateValue(av != bv, true);
      case SLE: return StateValue(av.sle(bv), true);
      case SLT: return StateValue(av.slt(bv), true);
      case SGE: return StateValue(av.sge(bv), true);
      case SGT: return StateValue(av.sgt(bv), true);
      case ULE: return StateValue(av.ule(bv), true);
      case ULT: return StateValue(av.ult(bv), true);
      case UGE: return StateValue(av.uge(bv), true);
      case UGT: return StateValue(av.ugt(bv), true);
      case Any:
        UNREACHABLE();
      }
      UNREACHABLE();
    };
  }

  auto scalar = [&](const StateValue &a, const StateValue &b) -> StateValue {
    auto fn2 = [&](Cond c) { return fn(a.value, b.value, c); };
    auto v = cond != Any ? fn2(cond) : build_icmp_chain(cond_var(), fn2);
    return { v.value.toBVBool(), a.non_poison && b.non_poison && v.non_poison };
  };

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
  os << getName() << " = fcmp ";
  if (flags & NNaN)
    os << "nnan ";
  if (flags & NInf)
    os << "ninf ";
  if (flags & Reassoc)
    os << "reassoc ";
  os << condtxt << print_type(getType()) << a->getName() << ", "
     << b->getName();
}

StateValue FCmp::toSMT(State &s) const {
  auto &a_eval = s[*a];
  auto &b_eval = s[*b];

  auto fn = [&](const auto &a, const auto &b) -> StateValue {
    const expr &av = a.value, &bv = b.value;
    expr val;
    switch (cond) {
    case OEQ: val = av.foeq(bv); break;
    case OGT: val = av.fogt(bv); break;
    case OGE: val = av.foge(bv); break;
    case OLT: val = av.folt(bv); break;
    case OLE: val = av.fole(bv); break;
    case ONE: val = av.fone(bv); break;
    case ORD: val = av.ford(bv); break;
    case UEQ: val = av.fueq(bv); break;
    case UGT: val = av.fugt(bv); break;
    case UGE: val = av.fuge(bv); break;
    case ULT: val = av.fult(bv); break;
    case ULE: val = av.fule(bv); break;
    case UNE: val = av.fune(bv); break;
    case UNO: val = av.funo(bv); break;
    }
    expr np = a.non_poison && b.non_poison;
    if (flags & NNaN)
      np &= !av.isNaN() && !bv.isNaN();
    if (flags & NInf)
      np &= !av.isInf() && !bv.isInf();
    if (flags & Reassoc)
      np &= expr(); // TODO

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
  return make_unique<FCmp>(getType(), getName() + suffix, cond, *a, *b, flags);
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

vector<Value*> Phi::operands() const {
  vector<Value*> v;
  for (auto &[val, bb] : values) {
    (void)bb;
    v.emplace_back(val);
  }
  return v;
}

void Phi::rauw(const Value &what, Value &with) {
  for (auto &[val, bb] : values) {
    (void)bb;
    RAUW(val);
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
  StateValue ret;
  bool first = true;

  for (auto &[val, bb] : values) {
    auto pre = s.jumpCondFrom(s.getFn().getBB(bb));
    if (!pre) // jump from unreachable BB
      continue;

    auto &v = s[*val];
    ret = first ? v : StateValue::mkIf(*pre, v, ret);
    first = false;
  }
  return ret;
}

expr Phi::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints();
  for (auto &[val, bb] : values) {
    (void)bb;
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
  if (auto br = dynamic_cast<Branch*>(instr))
    return idx == 0 ? br->getTrue() : *br->getFalse();

  if (auto sw = dynamic_cast<Switch*>(instr))
    return idx == 0 ? sw->getDefault() : sw->getTarget(idx-1).second;

  UNREACHABLE();
}

JumpInstr::target_iterator JumpInstr::it_helper::end() const {
  unsigned idx;
  if (auto br = dynamic_cast<Branch*>(instr)) {
    idx = br->getFalse() ? 2 : 1;
  } else if (auto sw = dynamic_cast<Switch*>(instr)) {
    idx = sw->getNumTargets() + 1;
  } else {
    UNREACHABLE();
  }
  return { instr, idx };
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
  os << "label " << dst_true.getName();
  if (dst_false)
    os << ", label " << dst_false->getName();
}

StateValue Branch::toSMT(State &s) const {
  if (cond) {
    s.addCondJump(s[*cond], dst_true, *dst_false);
  } else {
    s.addJump(dst_true);
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
    return make_unique<Branch>(*cond, dst_true, *dst_false);
  return make_unique<Branch>(dst_true);
}


void Switch::addTarget(Value &val, const BasicBlock &target) {
  targets.emplace_back(&val, target);
}

vector<Value*> Switch::operands() const {
  vector<Value*> ret = { value };
  for (auto &[val, target] : targets) {
    (void)target;
    ret.emplace_back(val);
  }
  return ret;
}

void Switch::rauw(const Value &what, Value &with) {
  RAUW(value);
  for (auto &[val, target] : targets) {
    (void)target;
    RAUW(val);
  }
}

void Switch::print(ostream &os) const {
  os << "switch " << *value << ", label " << default_target.getName() << " [\n";
  for (auto &[val, target] : targets) {
    os << "    " << *val << ", label " << target.getName() << '\n';
  }
  os << "  ]";
}

StateValue Switch::toSMT(State &s) const {
  auto &val = s[*value];
  expr default_cond(true);

  for (auto &[value_cond, bb] : targets) {
    auto &target = s[*value_cond];
    assert(target.non_poison.isTrue());
    s.addJump({ val.value == target.value, expr(val.non_poison) }, bb);
    default_cond &= val.value != target.value;
  }

  s.addJump({ move(default_cond), expr(val.non_poison) }, default_target);
  s.addUB(false);
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
  auto sw = make_unique<Switch>(*value, default_target);
  for (auto &[value_cond, bb] : targets) {
    sw->addTarget(*value_cond, bb);
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

StateValue Return::toSMT(State &s) const {
  s.addReturn(s[*val]);
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


vector<Value*> Assume::operands() const {
  return { cond };
}

void Assume::rauw(const Value &what, Value &with) {
  RAUW(cond);
}

void Assume::print(ostream &os) const {
  os << (if_non_poison ? "assume_non_poison " : "assume ") << *cond;
}

StateValue Assume::toSMT(State &s) const {
  auto &[v, np] = s[*cond];
  if (if_non_poison)
    s.addUB(np.implies(v != 0));
  else
    s.addUB(np && v != 0);
  return {};
}

expr Assume::getTypeConstraints(const Function &f) const {
  return cond->getType().enforceIntType();
}

unique_ptr<Instr> Assume::dup(const string &suffix) const {
  return make_unique<Assume>(*cond, if_non_poison);
}


vector<Value*> Alloc::operands() const {
  return { size };
}

void Alloc::rauw(const Value &what, Value &with) {
  RAUW(size);
}

void Alloc::print(std::ostream &os) const {
  os << getName() << " = alloca " << *size << ", align " << align;
}

StateValue Alloc::toSMT(State &s) const {
  auto &[sz, np] = s[*size];
  s.addUB(np);
  return { s.getMemory().alloc(sz, align, Memory::STACK), true };
}

expr Alloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size->getType().enforceIntType();
}

unique_ptr<Instr> Alloc::dup(const string &suffix) const {
  return make_unique<Alloc>(getType(), getName() + suffix, *size, align);
}


vector<Value*> Malloc::operands() const {
  return { size };
}

void Malloc::rauw(const Value &what, Value &with) {
  RAUW(size);
}

void Malloc::print(std::ostream &os) const {
  os << getName() << " = malloc " << *size;
}

StateValue Malloc::toSMT(State &s) const {
  auto &[sz, np] = s[*size];
  // TODO: malloc's alignment is implementation defined.
  auto p = s.getMemory().alloc(sz, 8, Memory::HEAP);

  if (isNonNull)
    return { move(p), expr(np) };

  auto nullp = Pointer::mkNullPointer(s.getMemory());
  auto flag = expr::mkFreshVar("malloc_isnull", expr(true));
  s.addQuantVar(flag);
  return { expr::mkIf(move(flag), nullp.release(), move(p)), expr(np) };
}

expr Malloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size->getType().enforceIntType();
}

unique_ptr<Instr> Malloc::dup(const string &suffix) const {
  return make_unique<Malloc>(getType(), getName() + suffix, *size, isNonNull);
}

vector<Value*> Calloc::operands() const {
  return { num, size };
}

void Calloc::rauw(const Value &what, Value &with) {
  RAUW(num);
  RAUW(size);
}

void Calloc::print(std::ostream &os) const {
  os << getName() << " = calloc " << *num << ", " << *size;
}

StateValue Calloc::toSMT(State &s) const {
  auto &[nm, np_num] = s[*num];
  auto &[sz, np_sz] = s[*size];
  expr calloc_sz = expr::mkIf(nm.mul_no_uoverflow(sz), expr(nm * sz),
                              expr::mkUInt(0, sz.bits()));

  // TODO: check calloc align.
  auto p = s.getMemory().alloc(nm * sz, 8, Memory::HEAP, std::nullopt,
                               nullptr, nm.mul_no_uoverflow(sz));
  
  // If memset's size is zero, then ptr can be NULL.
  s.getMemory().memset(p, { expr::mkUInt(0, 8), true }, calloc_sz, 1);

  auto nullp = Pointer::mkNullPointer(s.getMemory());
  auto flag = expr::mkFreshVar("calloc_isnull", expr(true));
  // TODO: We're moving from nondet. allocation to memory usage tracking, so
  // this part should be changed.
  s.addQuantVar(flag);
  return { expr::mkIf(move(flag), nullp.release(), move(p)),
           expr(np_num && np_sz) };
}

expr Calloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         num->getType().enforceIntType() &&
         size->getType().enforceIntType();
}

unique_ptr<Instr> Calloc::dup(const string &suffix) const {
  return make_unique<Calloc>(getType(), getName() + suffix, *num, *size,
                             isNonNull);
}

vector<Value*> Free::operands() const {
  return { ptr };
}

void Free::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Free::print(std::ostream &os) const {
  os << "free " << *ptr;
}

StateValue Free::toSMT(State &s) const {
  auto &[p, np] = s[*ptr];
  s.addUB(np);
  s.getMemory().free(p);
  return {};
}

expr Free::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Free::dup(const string &suffix) const {
  return make_unique<Free>(*ptr);
}


void GEP::addIdx(unsigned obj_size, Value &idx) {
  idxs.emplace_back(obj_size, &idx);
}

vector<Value*> GEP::operands() const {
  vector<Value*> v = { ptr };
  for (auto &[sz, idx] : idxs) {
    (void)sz;
    v.emplace_back(idx);
  }
  return v;
}

void GEP::rauw(const Value &what, Value &with) {
  RAUW(ptr);
  for (auto &[sz, idx] : idxs) {
    (void)sz;
    RAUW(idx);
  }
}

void GEP::print(std::ostream &os) const {
  os << getName() << " = gep ";
  if (inbounds)
    os << "inbounds ";
  os << *ptr;

  for (auto &[sz, idx] : idxs) {
    os << ", " << sz << " x " << *idx;
  }
}

StateValue GEP::toSMT(State &s) const {
  auto [val, non_poison] = s[*ptr];
  unsigned bits_offset = s.getMemory().bitsOffset();

  Pointer ptr(s.getMemory(), move(val));
  if (inbounds)
    non_poison &= ptr.inbounds();

  for (auto &[sz, idx] : idxs) {
    auto &[v, np] = s[*idx];
    auto multiplier = expr::mkUInt(sz, bits_offset);
    auto val = v.sextOrTrunc(bits_offset);
    auto inc = multiplier * val;

    if (inbounds) {
      non_poison &= multiplier.mul_no_soverflow(val);
      non_poison &= ptr.add_no_overflow(inc);
    }

    ptr += inc;
    non_poison &= np;

    if (inbounds)
      non_poison &= ptr.inbounds();
  }
  return { ptr.release(), move(non_poison) };
}

expr GEP::getTypeConstraints(const Function &f) const {
  auto c = Value::getTypeConstraints() &&
           getType() == ptr->getType() &&
           getType().enforcePtrType();
  for (auto &[sz, idx] : idxs) {
    (void)sz;
    c &= idx->getType().enforceIntType();
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


vector<Value*> Load::operands() const {
  return { ptr };
}

void Load::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Load::print(std::ostream &os) const {
  os << getName() << " = load " << getType() << ", " << *ptr
     << ", align " << align;
}

StateValue Load::toSMT(State &s) const {
  auto &[p, np] = s[*ptr];
  s.addUB(np);
  return s.getMemory().load(p, getType(), align);
}

expr Load::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Load::dup(const string &suffix) const {
  return make_unique<Load>(getType(), getName() + suffix, *ptr, align);
}


vector<Value*> Store::operands() const {
  return { val, ptr };
}

void Store::rauw(const Value &what, Value &with) {
  RAUW(val);
  RAUW(ptr);
}

void Store::print(std::ostream &os) const {
  os << "store " << *val << ", " << *ptr << ", align " << align;
}

StateValue Store::toSMT(State &s) const {
  auto &[p, np] = s[*ptr];
  s.addUB(np);
  s.getMemory().store(p, s[*val], val->getType(), align);
  return {};
}

expr Store::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Store::dup(const string &suffix) const {
  return make_unique<Store>(*ptr, *val, align);
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
  auto &[vptr, np_ptr] = s[*ptr];
  auto &[vbytes, np_bytes] = s[*bytes];
  s.addUB(vbytes.ugt(0).implies(np_ptr));
  s.addUB(np_bytes);
  s.getMemory().memset(vptr, s[*val].zextOrTrunc(8), vbytes, align);
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
  auto &[vdst, np_dst] = s[*dst];
  auto &[vsrc, np_src] = s[*src];
  auto &[vbytes, np_bytes] = s[*bytes];
  s.addUB(vbytes.ugt(0).implies(np_dst && np_src));
  s.addUB(np_bytes);
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
  return { v1, v2, mask };
}

void ShuffleVector::rauw(const Value &what, Value &with) {
  RAUW(v1);
  RAUW(v2);
  RAUW(mask);
}

void ShuffleVector::print(ostream &os) const {
  os << getName() << " = shufflevector " << *v1 << ", " << *v2 << ", " << *mask;
}

StateValue ShuffleVector::toSMT(State &s) const {
  auto vty = static_cast<const VectorType*>(v1->getType().getAsAggregateType());
  expr sz = expr::mkUInt(vty->numElementsConst(), 32);
  auto mty = mask->getType().getAsAggregateType();
  vector<StateValue> vals;

  for (unsigned i = 0, e = mty->numElementsConst(); i != e; ++i) {
    auto [iv, ip] = mty->extract(s[*mask], i);
    auto [lv, lp] = vty->extract(s[*v1], iv);
    auto [rv, rp] = vty->extract(s[*v2], iv - sz);
    expr v = expr::mkIf(iv.uge(sz), rv, lv);
    expr np = expr::mkIf(iv.uge(sz), rp, lp);
    expr inbounds = iv.ult(sz + sz);
    vals.emplace_back(move(v), ip && inbounds && np);
  }

  return getType().getAsAggregateType()->aggregateVals(vals);
}

expr ShuffleVector::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceVectorTypeSameChildTy(v1->getType()) &&
         getType().enforceVectorTypeEquiv(mask->getType()) &&
         v1->getType().enforceVectorType() &&
         v1->getType() == v2->getType() &&
         // mask is a vector of i32
         mask->getType().enforceVectorType([](auto &ty)
                                           { return ty.enforceIntType(32); });
}

unique_ptr<Instr> ShuffleVector::dup(const string &suffix) const {
  return make_unique<ShuffleVector>(getType(), getName() + suffix,
                                    *v1, *v2, *mask);
}

}
