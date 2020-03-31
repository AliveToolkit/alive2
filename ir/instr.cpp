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
#include <sstream>

using namespace smt;
using namespace util;
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


ostream& operator<<(ostream &os, const FastMathFlags &fm) {
  if (fm.flags == FastMathFlags::FastMath)
    return os << "fast ";

  if (fm.flags & FastMathFlags::NNaN)
    os << "nnan ";
  if (fm.flags & FastMathFlags::NInf)
    os << "ninf ";
  if (fm.flags & FastMathFlags::NSZ)
    os << "nsz ";
  if (fm.flags & FastMathFlags::ARCP)
    os << "arcp ";
  if (fm.flags & FastMathFlags::Contract)
    os << "contract ";
  if (fm.flags & FastMathFlags::Reassoc)
    os << "reassoc ";
  if (fm.flags & FastMathFlags::AFN)
    os << "afn ";
  return os;
}


BinOp::BinOp(Type &type, string &&name, Value &lhs, Value &rhs, Op op,
             unsigned flags, FastMathFlags fmath)
  : Instr(type, move(name)), lhs(&lhs), rhs(&rhs), op(op), flags(flags),
    fmath(fmath) {
  switch (op) {
  case Add:
  case Sub:
  case Mul:
  case Shl:
    assert((flags & (NSW | NUW)) == flags);
    assert(fmath.isNone());
    break;
  case SDiv:
  case UDiv:
  case AShr:
  case LShr:
    assert((flags & Exact) == flags);
    assert(fmath.isNone());
    break;
  case FAdd:
  case FSub:
  case FMul:
  case FDiv:
  case FRem:
    assert(flags == None);
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
    assert(flags == None);
    assert(fmath.isNone());
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
  os << fmath
     << print_type(getType()) << lhs->getName() << ", " << rhs->getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  s.addUB(bp);
  s.addUB(b != 0);
  if (sign)
    s.addUB((ap && a != expr::IntSMin(b.bits())) || b != expr::mkInt(-1, b));
}

static expr any_fp_zero(State &s, expr v) {
  expr is_zero = v.isFPZero();
  if (is_zero.isFalse())
    return v;

  expr var = expr::mkFreshVar("anyzero", true);
  s.addQuantVar(var);
  return expr::mkIf(is_zero,
                    expr::mkIf(var, expr::mkNumber("0", v),
                               expr::mkNumber("-0", v)),
                    v);
}

static StateValue fm_poison(State &s, expr a, expr b, expr c,
                            function<expr(expr&,expr&,expr&)> fn,
                            FastMathFlags fmath, bool only_input,
                            bool is_ternary = true) {
  if (fmath.flags & FastMathFlags::NSZ) {
    a = any_fp_zero(s, move(a));
    b = any_fp_zero(s, move(b));
    if (is_ternary)
      c = any_fp_zero(s, move(c));
  }

  expr val = fn(a, b, c);
  expr non_poison(true);

  if (fmath.flags & FastMathFlags::NNaN) {
    non_poison &= !a.isNaN() && !b.isNaN();
    if (is_ternary)
      non_poison &= !c.isNaN();
    if (!only_input)
      non_poison &= !val.isNaN();
  }
  if (fmath.flags & FastMathFlags::NInf) {
    non_poison &= !a.isInf() && !b.isInf();
    if (is_ternary)
      non_poison &= !c.isInf();
    if (!only_input)
      non_poison &= !val.isInf();
  }
  if (fmath.flags & FastMathFlags::ARCP)
    non_poison &= expr(); // TODO
  if (fmath.flags & FastMathFlags::Contract)
    non_poison &= expr(); // TODO
  if (fmath.flags & FastMathFlags::Reassoc)
    non_poison &= expr(); // TODO
  if (fmath.flags & FastMathFlags::AFN)
    non_poison &= expr(); // TODO
  if (fmath.flags & FastMathFlags::NSZ && !only_input)
    val = any_fp_zero(s, move(val));

  return { move(val), move(non_poison) };
}

static StateValue fm_poison(State &s, expr a, expr b,
                            function<expr(expr&,expr&)> fn,
                            FastMathFlags fmath, bool only_input) {
  return fm_poison(s, move(a), move(b), expr(),
                   [&](expr &a, expr &b, expr &c) { return fn(a, b); },
                   fmath, only_input, false);
}

StateValue BinOp::toSMT(State &s) const {
  bool vertical_zip = false;
  function<StateValue(const expr&, const expr&, const expr&, const expr&)>
    fn, scalar_op;

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
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.sadd_sat(b), true };
    };
    break;

  case UAdd_Sat:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.uadd_sat(b), true };
    };
    break;

  case SSub_Sat:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.ssub_sat(b), true };
    };
    break;

  case USub_Sat:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.usub_sat(b), true };
    };
    break;

  case And:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a & b, true };
    };
    break;

  case Or:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a | b, true };
    };
    break;

  case Xor:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a ^ b, true };
    };
    break;

  case Cttz:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.cttz(),
               b == 0u || a != 0u };
    };
    break;

  case Ctlz:
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a.ctlz(),
               b == 0u || a != 0u };
    };
    break;

  case SAdd_Overflow:
    vertical_zip = true;
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a + b, (!a.add_no_soverflow(b)).toBVBool() };
    };
    break;

  case UAdd_Overflow:
    vertical_zip = true;
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a + b, (!a.add_no_uoverflow(b)).toBVBool() };
    };
    break;

  case SSub_Overflow:
    vertical_zip = true;
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a - b, (!a.sub_no_soverflow(b)).toBVBool() };
    };
    break;

  case USub_Overflow:
    vertical_zip = true;
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a - b, (!a.sub_no_uoverflow(b)).toBVBool() };
    };
    break;

  case SMul_Overflow:
    vertical_zip = true;
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a * b, (!a.mul_no_soverflow(b)).toBVBool() };
    };
    break;

  case UMul_Overflow:
    vertical_zip = true;
    fn = [](auto a, auto ap, auto b, auto bp) -> StateValue {
      return { a * b, (!a.mul_no_uoverflow(b)).toBVBool() };
    };
    break;

  case FAdd:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return fm_poison(s, a, b, [](expr &a, expr &b) { return a.fadd(b); },
                       fmath, false);
    };
    break;

  case FSub:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return fm_poison(s, a, b, [](expr &a, expr &b) { return a.fsub(b); },
                       fmath, false);
    };
    break;

  case FMul:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return fm_poison(s, a, b, [](expr &a, expr &b) { return a.fmul(b); },
                       fmath, false);
    };
    break;

  case FDiv:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      return fm_poison(s, a, b, [](expr &a, expr &b) { return a.fdiv(b); },
                       fmath, false);
    };
    break;

  case FRem:
    fn = [&](auto a, auto ap, auto b, auto bp) -> StateValue {
      // TODO; Z3 has no support for LLVM's frem which is actually an fmod
      return fm_poison(s, a, b, [](expr &a, expr &b) { return expr(); }, fmath,
                       false);
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
  return make_unique<BinOp>(getType(), getName()+suffix, *lhs, *rhs, op, flags,
                            fmath);
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
  case IsConstant:  str = "is.constant"; break;
  case FNeg:        str = "fneg "; break;
  }

  os << getName() << " = " << str << fmath << print_type(getType())
     << val->getName();
}

StateValue UnaryOp::toSMT(State &s) const {
  function<expr(const expr&)> fn;

  switch (op) {
  case Copy:
    if (dynamic_cast<AggregateValue *>(val))
      // Aggregate value is not registered at state.
      return val->toSMT(s);
    return s[*val];
  case BitReverse:
    fn = [](auto v) { return v.bitreverse(); };
    break;
  case BSwap:
    fn = [](auto v) { return v.bswap(); };
    break;
  case Ctpop:
    fn = [](auto v) { return v.ctpop(); };
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
  case FNeg:
    // TODO
    if (!fmath.isNone())
      return {};
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
  expr instrconstr = getType() == val->getType();
  switch(op) {
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
    instrconstr &= getType().enforceIntOrVectorType();
    break;
  case IsConstant:
    instrconstr = getType().enforceIntType(1);
    break;
  case FNeg:
    instrconstr &= getType().enforceFloatOrVectorType();
    break;
  }

  return Value::getTypeConstraints() && move(instrconstr);
}

unique_ptr<Instr> UnaryOp::dup(const string &suffix) const {
  return make_unique<UnaryOp>(getType(), getName() + suffix, *val, op, fmath);
}


TernaryOp::TernaryOp(Type &type, string &&name, Value &a, Value &b, Value &c,
                     Op op, FastMathFlags fmath)
    : Instr(type, move(name)), a(&a), b(&b), c(&c), op(op), fmath(fmath) {
  switch (op) {
    case FShr:
    case FShl:
      assert(fmath.isNone());
      break;
    case FMA:
      break;
  }
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
  case FMA:
    str = "fma ";
    break;
  }

  os << getName() << " = " << str << fmath << *a << ", " << *b << ", " << *c;
}

StateValue TernaryOp::toSMT(State &s) const {
  auto &av = s[*a];
  auto &bv = s[*b];
  auto &cv = s[*c];
  function<StateValue(const expr&, const expr&, const expr&)> fn;

  switch (op) {
  case FShl:
    fn = [](auto a, auto b, auto c) -> StateValue {
      return { expr::fshl(a, b, c), true };
    };
    break;

  case FShr:
    fn = [](auto a, auto b, auto c) -> StateValue {
      return { expr::fshr(a, b, c), true };
    };
    break;

  case FMA:
    fn = [&](auto a, auto b, auto c) -> StateValue {
      return fm_poison(s, a, b, c, [](expr &a, expr &b, expr &c) {
                                   return expr::fma(a, b, c); }, fmath, false);
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
      auto [v, np] = fn(ai.value, bi.value, ci.value);
      vals.emplace_back(move(v), ai.non_poison && bi.non_poison &&
                                 ci.non_poison && np);
    }
    return ty->aggregateVals(vals);
  }
  auto [v, np] = fn(av.value, bv.value, cv.value);
  return { move(v), av.non_poison && bv.non_poison && cv.non_poison && np };
}

expr TernaryOp::getTypeConstraints(const Function &f) const {
  expr instrconstr = Value::getTypeConstraints() &&
                     getType() == a->getType() &&
                     getType() == b->getType() &&
                     getType() == c->getType();
  switch(op) {
    case FShl:
    case FShr:
      instrconstr &= getType().enforceIntOrVectorType();
      break;
    case FMA:
      instrconstr &= getType().enforceFloatOrVectorType();
      break;
  }
  return instrconstr;
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
  case FPExt:    str = "fpext "; break;
  case FPTrunc:  str = "fptrunc "; break;
  case Ptr2Int:  str = "ptrtoint "; break;
  case Int2Ptr:  str = "int2ptr "; break;
  }

  os << getName() << " = " << str << *val << print_type(getType(), " to ", "");
}

StateValue ConversionOp::toSMT(State &s) const {
  auto v = s[*val];
  function<StateValue(expr &&, const Type &)> fn;

  switch (op) {
  case SExt:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.sext(to_type.bits() - val.bits()), true };
    };
    break;
  case ZExt:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.zext(to_type.bits() - val.bits()), true };
    };
    break;
  case Trunc:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.trunc(to_type.bits()), true };
    };
    break;
  case BitCast:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { to_type.fromInt(move(val)), true };
    };
    break;
  case SIntToFP:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.sint2fp(to_type.getDummyValue(false).value), true };
    };
    break;
  case UIntToFP:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.uint2fp(to_type.getDummyValue(false).value), true };
    };
    break;
  case FPToSInt:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.fp2sint(to_type.bits()),
               val.foge(expr::IntSMin(to_type.bits()).sint2fp(val)) &&
               val.fole(expr::IntSMax(to_type.bits()).sint2fp(val)) &&
               !val.isInf() };
    };
    break;
  case FPToUInt:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.fp2uint(to_type.bits()),
               val.foge(expr::mkFloat(0, val)) &&
               val.fole(expr::IntUMax(to_type.bits()).uint2fp(val)) &&
               !val.isInf() };
    };
    break;
  case FPExt:
  case FPTrunc:
    fn = [](auto &&val, auto &to_type) -> StateValue {
      return { val.float2Float(to_type.getDummyValue(false).value), true };
    };
    break;
  case Ptr2Int:
    fn = [&](auto &&val, auto &to_type) -> StateValue {
      return { s.getMemory().ptr2int(val).zextOrTrunc(to_type.bits()), true };
    };
    break;
  case Int2Ptr:
    fn = [&](auto &&val, auto &to_type) -> StateValue {
      return { s.getMemory().int2ptr(val), true };
    };
    break;
  }

  auto scalar = [&](StateValue &&sv, const Type &to_type) -> StateValue {
    auto [v, np] = fn(move(sv.value), to_type);
    return { move(v), sv.non_poison && np };
  };

  if (op == BitCast)
    v = val->getType().toInt(s, move(v));

  if (getType().isVectorType()) {
    vector<StateValue> vals;
    auto retty = getType().getAsAggregateType();
    auto elems = retty->numElementsConst();

    // NOP: ptr vect -> ptr vect
    if (op == BitCast && retty->getChild(0).isPtrType())
      return v;

    // bitcast vect elems size may vary, so create a new data type whose
    // element size is aligned with the output vector elem size
    IntType elem_ty("int", retty->bits() / elems);
    VectorType int_ty("vec", elems, elem_ty);
    auto valty = op == BitCast ? &int_ty : val->getType().getAsAggregateType();

    for (unsigned i = 0; i != elems; ++i) {
      unsigned idx = (little_endian && op == BitCast) ? elems - i - 1 : i;
      vals.emplace_back(scalar(valty->extract(v, idx), retty->getChild(idx)));
    }
    return retty->aggregateVals(vals);
  }

  // turn poison data into boolean
  if (op == BitCast)
    v.non_poison = v.non_poison == 0;

  return scalar(move(v), getType());
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


void FnCall::addArg(Value &arg, unsigned flags) {
  args.emplace_back(&arg, flags);
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
  for (auto &[arg, flags] : args) {
    if (!first)
      os << ", ";

    if (flags & ArgByVal)
      os << "byval ";
    os << *arg;
    first = false;
  }
  os << ')';

  if (flags & NoRead)
    os << " noread";
  if (flags & NoWrite)
    os << " nowrite";
  if (flags & ArgMemOnly)
    os << " argmemonly";
  if (flags & NNaN)
    os << " NNaN";

  if (!valid)
    os << "\t; WARNING: unknown known function";
}

static void unpack_inputs(State&s, Type &ty, unsigned argflag,
                          const StateValue &value, vector<StateValue> &inputs,
                          vector<pair<StateValue, bool>> &ptr_inputs,
                          vector<StateValue> &returned_val) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      unpack_inputs(s, agg->getChild(i), argflag, agg->extract(value, i),
                    inputs, ptr_inputs, returned_val);
    }
  } else {
    if (argflag & FnCall::ArgReturned)
      returned_val.emplace_back(value);

    if (ty.isPtrType()) {
      Pointer p(s.getMemory(), value.value);
      p.strip_attrs();
      ptr_inputs.emplace_back(StateValue(p.release(), expr(value.non_poison)),
                              argflag & FnCall::ArgByVal);
    } else {
      inputs.emplace_back(value);
    }
  }
}

static void unpack_ret_ty (vector<Type*> &out_types, Type &ty) {
  if (auto agg = ty.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      unpack_ret_ty(out_types, agg->getChild(i));
    }
  } else {
    out_types.emplace_back(&ty);
  }
}

static StateValue pack_return(Type &ty, vector<StateValue> &vals,
                              unsigned flags, unsigned &idx) {
  if (auto agg = ty.getAsAggregateType()) {
    vector<StateValue> vs;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      vs.emplace_back(pack_return(agg->getChild(i), vals, flags, idx));
    }
    return agg->aggregateVals(vs);
  }

  auto ret = vals[idx++];
  if (ty.isFloatType() && (flags & FnCall::NNaN))
    ret.non_poison &= !ret.value.isNaN();
  return ret;
}

StateValue FnCall::toSMT(State &s) const {
  if (!valid) {
    s.addUB(expr());
    return {};
  }

  vector<StateValue> inputs;
  vector<pair<StateValue, bool>> ptr_inputs;
  vector<Type*> out_types;
  vector<StateValue> returned_val;

  ostringstream fnName_mangled;
  fnName_mangled << fnName;
  for (auto &[arg, flags] : args) {
    unpack_inputs(s, arg->getType(), flags, s[*arg], inputs, ptr_inputs,
                  returned_val);
    fnName_mangled << "#" << arg->getType().toString();
  }
  fnName_mangled << '!' << getType();
  if (!isVoid())
    unpack_ret_ty(out_types, getType());

  unsigned idx = 0;
  auto ret = s.addFnCall(fnName_mangled.str(), move(inputs), move(ptr_inputs),
                         out_types, !(flags & NoRead), !(flags & NoWrite),
                         flags & ArgMemOnly, move(returned_val));
  return isVoid() ? StateValue() : pack_return(getType(), ret, flags, idx);
}

expr FnCall::getTypeConstraints(const Function &f) const {
  // TODO : also need to name each arg type smt var uniquely
  return Value::getTypeConstraints();
}

unique_ptr<Instr> FnCall::dup(const string &suffix) const {
  auto r = make_unique<FnCall>(getType(), getName() + suffix, string(fnName),
                               flags, valid);
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
  os << getName() << " = fcmp " << fmath << condtxt << print_type(getType())
     << a->getName() << ", " << b->getName();
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
    auto [val, np] = fm_poison(s, a.value, b.value, cmp, fmath, true);
    return { val.toBVBool(), a.non_poison && b.non_poison && np };
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
  DisjointExpr<StateValue> ret;
  for (auto &[val, bb] : values) {
    // check if this was a jump from unreachable BB
    if (auto pre = s.jumpCondFrom(s.getFn().getBB(bb)))
      ret.add(s[*val], (*pre)());
  }
  return *ret();
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

static pair<expr,expr> // <condition, not_undef>
jump_undef_condition(State &s, const Value &val, const expr &e) {
  if (s.isUndef(e))
    return { expr::mkUInt(0, 1), false };

  expr c, a, b, lhs, rhs, ty;
  unsigned h, l;
  uint64_t n;

  // (ite (= ((_ extract 0 0) ty_%var) #b0) %var undef!0)
  if (e.isIf(c, a, b) && s.isUndef(b) && c.isEq(lhs, rhs)) {
    if (lhs.isUInt(n))
      swap(lhs, rhs);

    if (rhs.isUInt(n) && n == 0 &&
        lhs.isExtract(ty, h, l) && h == 0 && l == 0 && isTyVar(ty, a))
      return { move(a), c };
  }

  return { e, e == s[val].value };
}

StateValue Branch::toSMT(State &s) const {
  if (cond) {
    auto &c = s[*cond];
    auto [cond_val, not_undef] = jump_undef_condition(s, *cond, c.value);
    s.addUB(c.non_poison);
    s.addUB(not_undef);
    s.addCondJump(cond_val, dst_true, *dst_false);
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

  auto [cond_val, not_undef] = jump_undef_condition(s, *value, val.value);
  s.addUB(val.non_poison);
  s.addUB(not_undef);

  for (auto &[value_cond, bb] : targets) {
    auto &target = s[*value_cond];
    assert(target.non_poison.isTrue());
    expr cmp = cond_val == target.value;
    default_cond &= !cmp;
    s.addJump(move(cmp), bb);
  }

  s.addJump(move(default_cond), default_target);
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

static void addUBForNoCaptureRet(State &s, const StateValue &svret,
                                 const Type &t) {
  auto &[vret, npret] = svret;
  if (t.isPtrType()) {
    s.addUB(npret.implies(!Pointer(s.getMemory(), vret).is_nocapture()));
    return;
  }

  if (auto agg = t.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      addUBForNoCaptureRet(s, agg->extract(svret, i), agg->getChild(i));
    }
  }
}

StateValue Return::toSMT(State &s) const {
  // Encode nocapture semantics.
  auto &retval = s[*val];
  s.addUB(s.getMemory().check_nocapture());
  addUBForNoCaptureRet(s, retval, val->getType());
  s.addReturn(retval);
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
  if (mul)
    return { size, mul };
  return { size };
}

void Alloc::rauw(const Value &what, Value &with) {
  RAUW(size);
  RAUW(mul);
}

void Alloc::print(std::ostream &os) const {
  os << getName() << " = alloca " << *size;
  if (mul)
    os << " x " << *mul;
  os << ", align " << align;
  if (initially_dead)
    os << ", dead";
}

StateValue Alloc::toSMT(State &s) const {
  auto [sz, np] = s[*size];
  s.addUB(move(np));

  if (mul) {
    auto &[mul_e, mul_np] = s[*mul];
    s.addUB(mul_np);
    sz = sz.zextOrTrunc(bits_size_t);
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
  return make_unique<Alloc>(getType(), getName() + suffix, *size, mul, align,
                            initially_dead);
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

void Malloc::print(std::ostream &os) const {
  os << getName();
  if (!ptr)
    os << " = malloc ";
  else
    os << " = realloc " << *ptr << ", ";
  os << *size;
}

StateValue Malloc::toSMT(State &s) const {
  auto &[sz, np_size] = s.getAndAddUndefs(*size);
  unsigned align = heap_block_alignment;
  expr nonnull = expr::mkBoolVar("malloc_never_fails");
  auto [p_new, allocated] = s.getMemory().alloc(sz, align, Memory::MALLOC,
                                                np_size, nonnull);

  if (!ptr) {
    if (isNonNull) {
      // TODO: In C++ we need to throw an exception if the allocation fails,
      // but exception hasn't been modeled yet
      s.addPre(move(allocated));
    }
  } else {
    auto &[p, np_ptr] = s.getAndAddUndefs(*ptr);
    s.addUB(np_ptr);

    Pointer ptr(s.getMemory(), p);
    expr p_sz = ptr.block_size();
    expr sz_zext = sz.zextOrTrunc(p_sz.bits());

    expr memcpy_size = expr::mkIf(allocated,
                                  expr::mkIf(p_sz.ule(sz_zext), p_sz, sz_zext),
                                  expr::mkUInt(0, p_sz.bits()));

    s.getMemory().memcpy(p_new, p, memcpy_size, align, align, true);

    // 1) realloc(ptr, 0) always free the ptr.
    // 2) If allocation failed, we should not free previous ptr.
    expr nullp = Pointer::mkNullPointer(s.getMemory())();
    s.getMemory().free(expr::mkIf(sz == 0 || allocated, p, nullp), false);
  }
  return { move(p_new), expr(np_size) };
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
  auto &[nm, np_num] = s.getAndAddUndefs(*num);
  auto &[sz, np_sz] = s.getAndAddUndefs(*size);
  auto np = np_num && np_sz;

  unsigned align = heap_block_alignment;
  expr size = nm * sz;
  expr nonnull = expr::mkBoolVar("malloc_never_fails");
  auto [p, allocated] = s.getMemory().alloc(size, align, Memory::MALLOC,
                                            np && nm.mul_no_uoverflow(sz),
                                            nonnull);

  expr calloc_sz = expr::mkIf(allocated, size, expr::mkUInt(0, sz.bits()));
  s.getMemory().memset(p, { expr::mkUInt(0, 8), true }, calloc_sz, align, {});

  return { move(p), move(np) };
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


vector<Value*> StartLifetime::operands() const {
  return { ptr };
}

void StartLifetime::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void StartLifetime::print(std::ostream &os) const {
  os << "start_lifetime " << *ptr;
}

StateValue StartLifetime::toSMT(State &s) const {
  auto &[p, np] = s[*ptr];
  s.addUB(np);
  s.getMemory().start_lifetime(p);
  return {};
}

expr StartLifetime::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> StartLifetime::dup(const string &suffix) const {
  return make_unique<StartLifetime>(*ptr);
}


vector<Value*> Free::operands() const {
  return { ptr };
}

void Free::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Free::print(std::ostream &os) const {
  os << "free " << *ptr << (heaponly ? "" : " unconstrained");
}

StateValue Free::toSMT(State &s) const {
  auto &[p, np] = s[*ptr];
  s.addUB(np);
  // If not heaponly, don't encode constraints
  s.getMemory().free(p, !heaponly);
  return {};
}

expr Free::getTypeConstraints(const Function &f) const {
  return ptr->getType().enforcePtrType();
}

unique_ptr<Instr> Free::dup(const string &suffix) const {
  return make_unique<Free>(*ptr, heaponly);
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
        non_poison.add(ptr.add_no_overflow(inc));
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
    (void)sz;
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
  s.getMemory().store(p, s[*val], val->getType(), align, s.getUndefVars());
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
  s.addUB((vbytes != 0).implies(np_ptr));
  s.addUB(np_bytes);
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

  if (vbytes.bits() > bits_size_t)
    s.addUB(
      vbytes.ule(expr::IntUMax(bits_size_t).zext(vbytes.bits() - bits_size_t)));

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


vector<Value*> Strlen::operands() const {
  return { ptr };
}

void Strlen::rauw(const Value &what, Value &with) {
  RAUW(ptr);
}

void Strlen::print(ostream &os) const {
  os << getName() << " = strlen " << *ptr;
}

static pair<expr,expr>
find_null(State &s, Type &ty, const Pointer &ptr, AndExpr &prefix, unsigned i) {
  auto [val, ub0] = s.getMemory().load((ptr + i)(), IntType("i8", 8), 1);
  auto ub = ub0() && val.non_poison;

  auto is_zero = val.value == 0;
  auto result = expr::mkUInt(i, ty.bits());

  if (i == strlen_unroll_cnt - 1) {
    s.addPre(prefix().implies(is_zero), true);
    return { move(result), move(ub) };
  }

  if (is_zero.isTrue() || ub.isFalse())
    return { move(result), move(ub) };

  prefix.add(ub0);
  prefix.add(val.non_poison);
  prefix.add(!is_zero);
  auto [val2, ub2] = find_null(s, ty, ptr, prefix, i + 1);
  return { expr::mkIf(is_zero, result, val2),
           ub && (is_zero || ub2) };
}

StateValue Strlen::toSMT(State &s) const {
  auto &[eptr, np_ptr] = s[*ptr];
  s.addUB(np_ptr);
  AndExpr prefix;
  auto [val, ub]
    = find_null(s, getType(), Pointer(s.getMemory(), eptr), prefix, 0);
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

  if (dynamic_cast<UndefValue*>(mask))
    return UndefValue(getType()).toSMT(s);

  auto &vect1 = s[*v1];
  auto &vect2 = s[*v2];
  auto &mask_vector = static_cast<AggregateValue*>(mask)->getVals();

  for (unsigned i = 0, e = mty->numElementsConst(); i != e; ++i) {
    auto mask = mask_vector[i];
    // mask must be either a constant or undef
    // special case undef to yield undef
    if (dynamic_cast<UndefValue*>(mask)) {
      vals.emplace_back(UndefValue(vty->getChild(0)).toSMT(s));
      continue;
    }

    auto &[iv, ip] = s[*mask];
    assert(ip.isTrue());
    auto [lv, lp] = vty->extract(vect1, iv);
    auto [rv, rp] = vty->extract(vect2, iv - sz);

    expr val = expr::mkIf(iv.uge(sz), rv, lv);
    expr inbounds = iv.ult(sz + sz);
    if (!inbounds.isTrue())
      val = expr::mkIf(inbounds, val,
                       UndefValue(vty->getChild(0)).toSMT(s).value);

    vals.emplace_back(move(val), !inbounds || expr::mkIf(iv.uge(sz), rp, lp));
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
