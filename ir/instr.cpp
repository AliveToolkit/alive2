// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "ir/function.h"
#include "ir/type.h"
#include "smt/expr.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <functional>

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
             Flags flags)
  : Instr(type, move(name)), lhs(&lhs), rhs(&rhs), op(op), flags(flags) {
  switch (op) {
  case Add:
  case Sub:
  case Mul:
  case Shl:
    assert((flags & NSWNUW) == flags);
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
    assert((flags & NNANNINF) == flags);
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
  case Add:  str = "add"; break;
  case Sub:  str = "sub"; break;
  case Mul:  str = "mul"; break;
  case SDiv: str = "sdiv"; break;
  case UDiv: str = "udiv"; break;
  case SRem: str = "srem"; break;
  case URem: str = "urem"; break;
  case Shl:  str = "shl"; break;
  case AShr: str = "ashr"; break;
  case LShr: str = "lshr"; break;
  case SAdd_Sat: str = "sadd_sat"; break;
  case UAdd_Sat: str = "uadd_sat"; break;
  case SSub_Sat: str = "ssub_sat"; break;
  case USub_Sat: str = "usub_sat"; break;
  case And:  str = "and"; break;
  case Or:   str = "or"; break;
  case Xor:  str = "xor"; break;
  case Cttz: str = "cttz"; break;
  case Ctlz: str = "ctlz"; break;
  case SAdd_Overflow: str = "sadd_overflow"; break;
  case UAdd_Overflow: str = "uadd_overflow"; break;
  case SSub_Overflow: str = "ssub_overflow"; break;
  case USub_Overflow: str = "usub_overflow"; break;
  case SMul_Overflow: str = "smul_overflow"; break;
  case UMul_Overflow: str = "umul_overflow"; break;
  case FAdd: str = "fadd"; break;
  case FSub: str = "fsub"; break;
  case FMul: str = "fmul"; break;
  case FDiv: str = "fdiv"; break;
  case FRem: str = "frem"; break;
  }

  const char *flag = nullptr;
  switch (flags) {
  case None:   flag = " "; break;
  case NSW:    flag = " nsw "; break;
  case NUW:    flag = " nuw "; break;
  case NSWNUW: flag = " nsw nuw "; break;
  case NNAN:   flag = " nnan "; break;
  case NINF:   flag = " ninf "; break;
  case NNANNINF: flag = " nnan ninf "; break;
  case Exact:  flag = " exact "; break;
  }

  os << getName() << " = " << str << flag << print_type(getType())
     << lhs->getName() << ", " << rhs->getName();
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
                      BinOp::Flags flags) {
  expr ret = false;
  if (flags & BinOp::NINF)
    ret |= a.isInf() || b.isInf() || val.isInf();
  if (flags & BinOp::NNAN)
    ret |= a.isNaN() || b.isNaN() || val.isNaN();

  return ret;
}

StateValue BinOp::toSMT(State &s) const {
  std::function<StateValue(const StateValue&,
                           const StateValue&, Type&)> scalar_op;

  switch (op) {
  case Add:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      if (flags & NSW)
        non_poison &= a.value.add_no_soverflow(b.value);
      if (flags & NUW)
        non_poison &= a.value.add_no_uoverflow(b.value);
      return { a.value + b.value, move(non_poison) };
    };
    break;

  case Sub:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      if (flags & NSW)
        non_poison &= a.value.sub_no_soverflow(b.value);
      if (flags & NUW)
        non_poison &= a.value.sub_no_uoverflow(b.value);
      return { a.value - b.value, move(non_poison) };
    };
    break;

  case Mul:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      if (flags & NSW)
        non_poison &= a.value.sub_no_soverflow(b.value);
      if (flags & NUW)
        non_poison &= a.value.sub_no_uoverflow(b.value);
      return { a.value * b.value, move(non_poison) };
    };
    break;

  case SDiv:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      div_ub(s, a.value, b.value, a.non_poison, b.non_poison, true);
      if (flags & Exact)
        non_poison &= a.value.sdiv_exact(b.value);
      return { a.value.sdiv(b.value), move(non_poison) };
    };
    break;

  case UDiv:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      div_ub(s, a.value, b.value, a.non_poison, b.non_poison, false);
      if (flags & Exact)
        non_poison &= a.value.udiv_exact(b.value);
      return { a.value.udiv(b.value), move(non_poison) };
    };
    break;

  case SRem:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      div_ub(s, a.value, b.value, a.non_poison, b.non_poison, true);
      return { a.value.srem(b.value), move(non_poison) };
    };
    break;

  case URem:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      div_ub(s, a.value, b.value, a.non_poison, b.non_poison, false);
      return { a.value.urem(b.value), move(non_poison) };
    };
    break;

  case Shl:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= b.value.ult(b.value.bits());
      if (flags & NSW)
        non_poison &= a.value.shl_no_soverflow(b.value);
      if (flags & NUW)
        non_poison &= a.value.shl_no_uoverflow(b.value);

      return { a.value << b.value, move(non_poison) };
    };
    break;

  case AShr:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= b.value.ult(b.value.bits());
      if (flags & Exact)
        non_poison &= a.value.ashr_exact(b.value);
      return { a.value.ashr(b.value), move(non_poison) };
    };
    break;

  case LShr:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= b.value.ult(b.value.bits());
      if (flags & Exact)
        non_poison &= a.value.lshr_exact(b.value);
      return { a.value.lshr(b.value), move(non_poison) };
    };
    break;

  case SAdd_Sat:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value.sadd_sat(b.value),  a.non_poison && b.non_poison };
    };
    break;

  case UAdd_Sat:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value.uadd_sat(b.value),  a.non_poison && b.non_poison };
    };
    break;

  case SSub_Sat:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value.ssub_sat(b.value),  a.non_poison && b.non_poison };
    };
    break;

  case USub_Sat:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value.usub_sat(b.value),  a.non_poison && b.non_poison };
    };
    break;

  case And:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value & b.value,  a.non_poison && b.non_poison };
    };
    break;

  case Or:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value | b.value,  a.non_poison && b.non_poison };
    };
    break;

  case Xor:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      return { a.value ^ b.value,  a.non_poison && b.non_poison };
    };
    break;

  case Cttz:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= (b.value == 0u || a.value != 0u);
      return { a.value.cttz(), move(non_poison) };
    };
    break;

  case Ctlz:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= (b.value == 0u || a.value != 0u);
      return { a.value.ctlz(), move(non_poison) };
    };
    break;

  case SAdd_Overflow:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      vector<StateValue> vals = { { a.value + b.value, expr(non_poison) } };
      vals.emplace_back((!a.value.add_no_soverflow(b.value)).toBVBool(), move(non_poison));
      return t.getAsAggregateType()->aggregateVals(vals);
    };
    break;

  case UAdd_Overflow:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      vector<StateValue> vals = { { a.value + b.value, expr(non_poison) } };
      vals.emplace_back((!a.value.add_no_uoverflow(b.value)).toBVBool(), move(non_poison));
      return t.getAsAggregateType()->aggregateVals(vals);
    };
    break;

  case SSub_Overflow:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      vector<StateValue> vals = { { a.value - b.value, expr(non_poison) } };
      vals.emplace_back((!a.value.sub_no_soverflow(b.value)).toBVBool(), move(non_poison));
      return t.getAsAggregateType()->aggregateVals(vals);
    };
    break;

  case USub_Overflow:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      vector<StateValue> vals = { { a.value - b.value, expr(non_poison) } };
      vals.emplace_back((!a.value.sub_no_uoverflow(b.value)).toBVBool(), move(non_poison));
      return t.getAsAggregateType()->aggregateVals(vals);
    };
    break;

  case SMul_Overflow:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      vector<StateValue> vals = { { a.value * b.value, expr(non_poison) } };
      vals.emplace_back((!a.value.mul_no_soverflow(b.value)).toBVBool(), move(non_poison));
      return t.getAsAggregateType()->aggregateVals(vals);
    };
    break;

  case UMul_Overflow:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      vector<StateValue> vals = { { a.value * b.value, expr(non_poison) } };
      vals.emplace_back((!a.value.mul_no_uoverflow(b.value)).toBVBool(), move(non_poison));
      return t.getAsAggregateType()->aggregateVals(vals);
    };
    break;

  case FAdd:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= !fm_poison(a.value, b.value, a.value.fadd(b.value), flags);
      return { a.value.fadd(b.value), move(non_poison) };
    };
    break;

  case FSub:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= !fm_poison(a.value, b.value, a.value.fsub(b.value), flags);
      return { a.value.fsub(b.value), move(non_poison) };
    };
    break;

  case FMul:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= !fm_poison(a.value, b.value, a.value.fmul(b.value), flags);
      return { a.value.fmul(b.value), move(non_poison) };
    };
    break;

  case FDiv:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= !fm_poison(a.value, b.value, a.value.fdiv(b.value), flags);
      return { a.value.fdiv(b.value), move(non_poison) };
    };
    break;

  case FRem:
    scalar_op = [&](auto a, auto b, auto &t) -> StateValue {
      auto non_poison = a.non_poison && b.non_poison;
      non_poison &= !fm_poison(a.value, b.value, expr(), flags);
      // TODO; Z3 has no support for LLVM's frem which is actually an fmod
      return { expr(), move(non_poison) };
    };
    break;
  }

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto aty = getType().getAsAggregateType();
    for (unsigned i = 0 ; i < aty->numElementsConst(); ++i) {
      auto a_i = aty->extract(s[*lhs], i);
      auto b_i = aty->extract(s[*rhs], i);
      auto &cty = aty->getChild(i);
      vals.emplace_back(scalar_op(move(a_i), move(b_i), cty));
    }
    return aty->aggregateVals(vals);
  }

  return scalar_op(move(s[*lhs]), move(s[*rhs]), getType());
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
                  lhs->getType().enforceIntType() &&
                  lhs->getType() == rhs->getType();

    if (auto ty = getType().getAsStructType()) {
      instrconstr &= ty->numElements() == 2 &&
                     ty->getChild(0) == lhs->getType() &&
                     ty->getChild(1).enforceIntType(1);
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
    instrconstr = getType().enforceFloatType() &&
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
  auto &[v, vp] = s[*val];
  expr newval;

  switch (op) {
  case Copy:
    newval = v;
    break;
  case BitReverse:
    newval = v.bitreverse();
    break;
  case BSwap:
    newval = v.bswap();;
    break;
  case Ctpop:
    newval = v.ctpop();
    break;
  case FNeg:
    newval = v.fneg();
    break;
  }
  return { move(newval), expr(vp) };
}

expr UnaryOp::getTypeConstraints(const Function &f) const {
  expr instrconstr = true;
  switch(op) {
  case BSwap:
    instrconstr = val->getType().sizeVar().urem(expr::mkUInt(16, 8)) == 0;
    [[fallthrough]];
  case BitReverse:
  case Ctpop:
    instrconstr &= getType().enforceIntOrVectorType();
  case Copy:
    break;
  case FNeg:
    instrconstr = getType().enforceFloatType();
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
  auto &[av, ap] = s[*a];
  auto &[bv, bp] = s[*b];
  auto &[cv, cp] = s[*c];
  expr newval;
  expr not_poison = ap && bp && cp;

  switch (op) {
  case FShl:
    newval = expr::fshl(av, bv, cv);
    break;
  case FShr:
    newval = expr::fshr(av, bv, cv);
    break;
  }

  return { move(newval), move(not_poison) };
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
  auto &[v, vp] = s[*val];
  expr newval;
  auto to_bw = getType().bits();

  switch (op) {
  case SExt:
    newval = v.sext(to_bw - val->bits());
    break;
  case ZExt:
    newval = v.zext(to_bw - val->bits());
    break;
  case Trunc:
    newval = v.trunc(to_bw);
    break;
  case BitCast:
    if ((getType().isIntType() && val->getType().isIntType()) ||
        (getType().isFloatType() && val->getType().isFloatType()) ||
        (getType().isPtrType() && val->getType().isPtrType()))
      newval = v;
    else if (getType().isIntType() && val->getType().isFloatType())
      newval = v.float2BV();
    else if (getType().isFloatType() && val->getType().isIntType())
      newval = v.BV2float(getType().getDummyValue(true).value);
    else
      UNREACHABLE();
    break;
  case SIntToFP:
    newval = v.sint2fp(getType().getDummyValue(false).value);
    break;
  case UIntToFP:
    newval = v.uint2fp(getType().getDummyValue(false).value);
    break;
  case FPToSInt:
    newval = v.fp2sint(to_bw);
    break;
  case FPToUInt:
    newval = v.fp2uint(to_bw);
    break;
  case Ptr2Int:
    newval = s.getMemory().ptr2int(v).zextOrTrunc(getType().bits());
    break;
  case Int2Ptr:
    newval = s.getMemory().int2ptr(v);
    break;
  }
  return { move(newval), expr(vp) };
}

expr ConversionOp::getTypeConstraints(const Function &f) const {
  expr c;
  switch (op) {
  case SExt:
  case ZExt:
    c = getType().enforceIntOrVectorType() &&
        getType().sameType(val->getType()) &&
        val->getType().sizeVar().ult(getType().sizeVar());
    break;
  case Trunc:
    c = getType().enforceIntOrVectorType() &&
        getType().sameType(val->getType()) &&
        getType().sizeVar().ult(val->getType().sizeVar());
    break;
  case BitCast:
    // FIXME: input can only be ptr if result is a ptr as well
    c = getType().enforceIntOrPtrOrVectorType() &&
        getType().sizeVar() == val->getType().sizeVar();
    break;
  case SIntToFP:
  case UIntToFP:
    // FIXME: output is vector iff input is vector
    c = getType().enforceFloatOrVectorType() &&
        val->getType().enforceIntOrVectorType();
    break;
  case FPToSInt:
  case FPToUInt:
    c = getType().enforceIntOrVectorType() &&
        val->getType().enforceFloatOrVectorType();
    break;
  case Ptr2Int:
    c = getType().enforceIntType() &&
        val->getType().enforcePtrType();
    break;
  case Int2Ptr:
    c = getType().enforcePtrType() &&
        val->getType().enforceIntType();
    break;
  }
  return Value::getTypeConstraints() && move(c);
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
  auto &[c, cp] = s[*cond];
  auto &[av, ap] = s[*a];
  auto &[bv, bp] = s[*b];
  auto cond = c == 1u;
  return { expr::mkIf(cond, av, bv),
           cp && expr::mkIf(cond, ap, bp) };
}

expr Select::getTypeConstraints(const Function &f) const {
  return cond->getType().enforceIntType(1) &&
         Value::getTypeConstraints() &&
         getType().enforceIntOrPtrOrVectorType() &&
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
    auto var_name = fnName + '#' + fresh_id();
    auto var = expr::mkVar(var_name.c_str(), ret_type.value);
    s.addQuantVar(var);
    val = expr::mkIf(all_args_np, val, var);
  }

  // FIXME: broken for functions that return aggregate types
  auto poison_name = fnName + "#poison";
  add_implies_axiom(poison_name);

  return { move(val), expr::mkUF(poison_name.c_str(), all_args, false) };
}

expr FnCall::getTypeConstraints(const Function &f) const {
  // TODO : also need to name each arg type smt var uniquely
  return Value::getTypeConstraints();
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
                                   function<StateValue(ICmp::Cond)> &fn,
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
  auto &av = a_eval.value, &bv = b_eval.value;
  function<StateValue(Cond)> fn;
  expr val, non_poison = a_eval.non_poison && b_eval.non_poison;

  if (a->getType().isPtrType()) {
    Pointer lhs(s.getMemory(), av);
    Pointer rhs(s.getMemory(), bv);

    fn = [&](Cond cond) {
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
    fn = [&](Cond cond) {
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

  StateValue cmp = cond != Any ? fn(cond) : build_icmp_chain(cond_var(), fn);
  return { cmp.value.toBVBool(), move(non_poison) && move(cmp.non_poison) };
}

expr ICmp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType(1) &&
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
  os << getName() << " = fcmp " << condtxt << print_type(getType())
     << a->getName() << ", " << b->getName();
}

StateValue FCmp::toSMT(State &s) const {
  auto &[av, ap] = s[*a];
  auto &[bv, bp] = s[*b];
  expr val;
  switch (cond) {
  case OEQ:   val = av.foeq(bv); break;
  case OGT:   val = av.fogt(bv); break;
  case OGE:   val = av.foge(bv); break;
  case OLT:   val = av.folt(bv); break;
  case OLE:   val = av.fole(bv); break;
  case ONE:   val = av.fone(bv); break;
  case ORD:   val = av.ford(bv); break;
  case UEQ:   val = av.fueq(bv); break;
  case UGT:   val = av.fugt(bv); break;
  case UGE:   val = av.fuge(bv); break;
  case ULT:   val = av.fult(bv); break;
  case ULE:   val = av.fule(bv); break;
  case UNE:   val = av.fune(bv); break;
  case UNO:   val = av.funo(bv); break;
  }
  return { val.toBVBool(), ap && bp };
}

expr FCmp::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType(1) &&
         a->getType().enforceFloatType() &&
         a->getType() == b->getType();
}

unique_ptr<Instr> FCmp::dup(const string &suffix) const {
  return make_unique<FCmp>(getType(), getName() + suffix, cond, *a, *b);
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
  auto &[v, p] = s[*val];
  s.resetUndefVars();

  if (p.isTrue())
    return { expr(v), expr(p) };

  auto name = "nondet_" + fresh_id();
  StateValue ret_type = getType().getDummyValue(true);
  expr nondet = expr::mkVar(name.c_str(), ret_type.value);
  s.addQuantVar(nondet);

  return { expr::mkIf(p, v, move(nondet)), move(ret_type.non_poison) };
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

    auto v = s[*val];
    if (first) {
      ret = v;
      first = false;
    } else {
      ret = StateValue::mkIf(*pre, v, ret);
    }
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
  auto val = s[*value];
  expr default_cond(true);

  for (auto &[value_cond, bb] : targets) {
    auto target = s[*value_cond];
    assert(target.non_poison.isTrue());
    s.addJump({ val.value == target.value, expr(val.non_poison) }, bb);
    default_cond &= val.value != target.value;
  }

  s.addJump({ move(default_cond), move(val.non_poison) }, default_target);
  s.addUB(false);
  return {};
}

expr Switch::getTypeConstraints(const Function &f) const {
  expr typ = value->getType().enforceIntType();
  for (auto &p : targets) {
    typ &= p.first->getType().enforceIntType();
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
  if (&getType() != &Type::voidTy)
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
  return { s.getMemory().alloc(sz, align, false), true };
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
  s.addUB(np);
  // TODO: malloc's alignment is implementation defined.
  return { s.getMemory().alloc(sz, 8, true), true };
}

expr Malloc::getTypeConstraints(const Function &f) const {
  return Value::getTypeConstraints() &&
         getType().enforcePtrType() &&
         size->getType().enforceIntType();
}

unique_ptr<Instr> Malloc::dup(const string &suffix) const {
  return make_unique<Malloc>(getType(), getName() + suffix, *size);
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
  auto c = getType() == ptr->getType() &&
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

}
