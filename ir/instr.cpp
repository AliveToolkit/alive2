// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "smt/expr.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;

namespace IR {

BinOp::BinOp(unique_ptr<Type> &&type, string &&name, Value &lhs, Value &rhs,
             Op op, Flags flags)
  : Instr(move(type), move(name)), lhs(lhs), rhs(rhs), op(op), flags(flags) {
  getWType().enforceIntOrPtrOrVectorType();

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
  case SRem:
  case URem:
  case And:
  case Or:
  case Xor:
    assert(flags == 0);
    break;
  }
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
  case And:  str = "and"; break;
  case Or:   str = "or"; break;
  case Xor:  str = "xor"; break;
  }

  const char *flag = nullptr;
  switch (flags) {
  case None:   flag = " "; break;
  case NSW:    flag = " nsw "; break;
  case NUW:    flag = " nuw "; break;
  case NSWNUW: flag = " nsw nuw "; break;
  case Exact:  flag = " exact "; break;
  }

  os << getName() << " = " << str << flag << lhs << ", " << rhs.getName();
}

static void div_ub(State &s, const expr &a, const expr &b, const expr &ap,
                   const expr &bp, bool sign) {
  auto bits = b.bits();
  s.addUB(bp);
  s.addUB(b != expr::mkUInt(0, bits));
  if (sign)
    s.addUB((ap && a != expr::IntMin(bits)) || b != expr::mkInt(-1, bits));
}

StateValue BinOp::toSMT(State &s) const {
  auto &[a, ap] = s[lhs];
  auto &[b, bp] = s[rhs];
  expr val;
  auto not_poison = ap && bp;

  switch (op) {
  case Add:
    val = a + b;
    if (flags & NSW)
      not_poison &= a.add_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.add_no_uoverflow(b);
    break;

  case Sub:
    val = a - b;
    if (flags & NSW)
      not_poison &= a.sub_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.sub_no_uoverflow(b);
    break;

  case Mul:
    val = a * b;
    if (flags & NSW)
      not_poison &= a.mul_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.mul_no_uoverflow(b);
    break;

  case SDiv:
    val = a.sdiv(b);
    div_ub(s, a, b, ap, bp, true);

    if (flags & Exact)
      not_poison &= a.sdiv_exact(b);
    break;

  case UDiv:
    val = a.udiv(b);
    div_ub(s, a, b, ap, bp, false);

    if (flags & Exact)
      not_poison &= a.udiv_exact(b);
    break;

  case SRem:
    val = a.srem(b);
    div_ub(s, a, b, ap, bp, true);
    break;

  case URem:
    val = a.urem(b);
    div_ub(s, a, b, ap, bp, false);
    break;

  case Shl:
  {
    val = a << b;

    auto bits = b.bits();
    not_poison &= b.ult(expr::mkUInt(bits, bits));

    if (flags & NSW)
      not_poison &= a.shl_no_soverflow(b);
    if (flags & NUW)
      not_poison &= a.shl_no_uoverflow(b);
    break;
  }
  case AShr: {
    val = a.ashr(b);

    auto bits = b.bits();
    not_poison &= b.ult(expr::mkUInt(bits, bits));

    if (flags & Exact)
      not_poison &= a.ashr_exact(b);
    break;
  }
  case LShr: {
    val = a.lshr(b);

    auto bits = b.bits();
    not_poison &= b.ult(expr::mkUInt(bits, bits));

    if (flags & Exact)
      not_poison &= a.lshr_exact(b);
    break;
  }
  case And:
    val = a & b;
    break;

  case Or:
    val = a | b;
    break;

  case Xor:
    val = a ^ b;
    break;
  }
  return { move(val), move(not_poison) };
}

expr BinOp::getTypeConstraints() const {
  return getType().getTypeConstraints() &&
         getType() == lhs.getType() &&
         getType() == rhs.getType();
}


ConversionOp::ConversionOp(unique_ptr<Type> &&type, string &&name, Value &val,
                           Op op) :
  Instr(move(type), move(name)), val(val), op(op) {
  getWType().enforceIntOrPtrOrVectorType();
  val.getWType().enforceIntOrPtrOrVectorType();
}

void ConversionOp::print(std::ostream &os) const {
  const char *str = nullptr;
  switch (op) {
  case SExt:  str = "sext "; break;
  case ZExt:  str = "zext "; break;
  case Trunc: str = "trunc "; break;
  }

  os << getName() << " = " << str << val;

  if (auto ty = getType().toString();
      !ty.empty())
    os << " to " << ty;
}

StateValue ConversionOp::toSMT(State &s) const {
  auto &[v, vp] = s[val];
  expr newval;
  auto to_bw = getType().bits();

  switch (op) {
  case SExt:
    newval = v.sext(to_bw - val.bits());
    break;
  case ZExt:
    newval = v.zext(to_bw - val.bits());
    break;
  case Trunc:
    newval = v.trunc(to_bw);
    break;
  }
  return { move(newval), expr(vp) };
}

expr ConversionOp::getTypeConstraints() const {
  expr sizeconstr;
  switch (op) {
  case SExt:
  case ZExt:
    sizeconstr = val.getType().sizeVar().ult(getType().sizeVar());
    break;
  case Trunc:
    sizeconstr = getType().sizeVar().ult(val.getType().sizeVar());
    break;
  }
  // TODO: missing constraint comparing ty and to_ty (e.g. both ints)
  return getType().getTypeConstraints() &&
         val.getTypeConstraints() &&
         move(sizeconstr);
}


Select::Select(unique_ptr<Type> &&type, string &&name, Value &cond, Value &a,
               Value &b)
  : Instr(move(type), move(name)), cond(cond), a(a), b(b) {
  getWType().enforceIntOrPtrOrVectorType();
  cond.getWType().enforceIntType();
  a.getWType().enforceIntOrPtrOrVectorType();
  b.getWType().enforceIntOrPtrOrVectorType();
}

void Select::print(std::ostream &os) const {
  os << getName() << " = select " << cond << ", " << a << ", " << b;
}

StateValue Select::toSMT(State &s) const {
  auto &[c, cp] = s[cond];
  auto &[av, ap] = s[a];
  auto &[bv, bp] = s[b];
  auto cond = c == 1u;
  return { expr::mkIf(cond, av, bv),
           cp && expr::mkIf(cond, ap, bp) };
}

expr Select::getTypeConstraints() const {
  return cond.getTypeConstraints() &&
         cond.getType().sizeVar() == 1u &&
         getType().getTypeConstraints() &&
         getType() == a.getType() &&
         getType() == b.getType();
}


void Return::print(ostream &os) const {
  os << "ret " << val;
}

StateValue Return::toSMT(State &s) const {
  s.addReturn(s[val]);
  return {};
}

expr Return::getTypeConstraints() const {
  return getType().getTypeConstraints() &&
         getType() == val.getType();
}


void Unreachable::print(ostream &os) const {
  os << "unreachable";
}

StateValue Unreachable::toSMT(State &s) const {
  s.addUB(false);
  return {};
}

expr Unreachable::getTypeConstraints() const {
  return true;
}

}
