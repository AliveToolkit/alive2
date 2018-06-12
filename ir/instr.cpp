// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/instr.h"
#include "smt/expr.h"
#include "util/compiler.h"

using namespace smt;
using namespace std;

namespace IR {

void Instr::printType(ostream &os) const {
  auto t = getType().toString();
  if (!t.empty())
    os << t << ' ';
}


BinOp::BinOp(unique_ptr<Type> &&type, string &&name, Value &lhs, Value &rhs,
             Op op, Flags flags)
  : Instr(move(type), move(name)), lhs(lhs), rhs(rhs), op(op), flags(flags) {
  getWType().enforceIntOrPtrOrVectorType();

  switch (op) {
  case Add:  assert((flags & NSWNUW) == flags); break;
  case Sub:  assert((flags & NSWNUW) == flags); break;
  case Mul:  assert((flags & NSWNUW) == flags); break;
  case SDiv: assert((flags & Exact) == flags); break;
  case UDiv: assert((flags & Exact) == flags); break;
  case Shl:  assert((flags & NSWNUW) == flags); break;
  case AShr: assert((flags & Exact) == flags); break;
  case LShr: assert((flags & Exact) == flags); break;
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
  case Shl:  str = "shl"; break;
  case AShr: str = "ashr"; break;
  case LShr: str = "lshr"; break;
  }

  const char *flag = nullptr;
  switch (flags) {
  case None:   flag = " "; break;
  case NSW:    flag = " nsw "; break;
  case NUW:    flag = " nuw "; break;
  case NSWNUW: flag = " nsw nuw "; break;
  case Exact:  flag = " exact "; break;
  }

  os << getName() << " = " << str << flag;
  printType(os);
  os << lhs.getName() << ", " << rhs.getName();
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
  {
    val = a.sdiv(b);
    if (flags & Exact)
      not_poison &= a.sdiv_exact(b);

    auto bits = b.bits();
    s.addUB(bp);
    s.addUB(b != expr::mkUInt(0, bits));
    s.addUB((ap && a != expr::IntMin(bits)) || b != expr::mkInt(-1, bits));
    break;
  }
  case UDiv:
    val = a.udiv(b);
    if (flags & Exact)
      not_poison &= a.udiv_exact(b);

    s.addUB(bp);
    s.addUB(b != expr::mkUInt(0, b.bits()));
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
  }
  return { move(val), move(not_poison) };
}

expr BinOp::getTypeConstraints() const {
  return getType().getTypeConstraints() &&
         getType() == lhs.getType() &&
         getType() == rhs.getType();
}


void Return::print(ostream &os) const {
  os << "ret ";
  printType(os);
  os << val.getName();
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
