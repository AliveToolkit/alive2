// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include "ir/constant.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include "util/config.h"

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

VoidValue Value::voidVal;

bool Value::isVoid() const {
  return &getType() == &Type::voidTy;
}

expr Value::getTypeConstraints() const {
  return getType().getTypeConstraints();
}

void Value::fixupTypes(const Model &m) {
  type.fixup(m);
}

bool Value::isInt(int64_t &i) const {
  auto ci = dynamic_cast<const IntConst *>(this);
  if (ci) {
    if (auto ii = ci->getInt())
      i = *ii;
  }
  return ci;
}

ostream& operator<<(ostream &os, const Value &val) {
  auto t = val.getType().toString();
  os << t;
  if (!val.isVoid()) {
    if (!t.empty()) os << ' ';
    os << val.getName();
  }
  return os;
}


void UndefValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue UndefValue::toSMT(State &s) const {
  auto val = getType().getDummyValue(true);
  expr var = expr::mkFreshVar("undef", val.value);
  s.addUndefVar(expr(var));
  return { move(var), move(val.non_poison) };
}


void PoisonValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue PoisonValue::toSMT(State &s) const {
  return getType().getDummyValue(false);
}


void VoidValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue VoidValue::toSMT(State &s) const {
  return { false, false };
}


void NullPointerValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue NullPointerValue::toSMT(State &s) const {
  auto nullp = Pointer::mkNullPointer(s.getMemory());
  return { nullp.release(), true };
}


void GlobalVariable::print(ostream &os) const {
  os << getName() << " = " << (isconst ? "constant " : "global ") << allocsize
     << " bytes, align " << align;
}

StateValue GlobalVariable::toSMT(State &s) const {
  auto sizeexpr = expr::mkUInt(allocsize, 64);
  expr ptrval;
  unsigned glbvar_bid;
  auto blkkind = isconst ? Memory::CONSTGLOBAL : Memory::GLOBAL;

  if (s.hasGlobalVarBid(getName(), glbvar_bid)) {
    // Use the same block id that is used by src
    assert(!s.isSource());
    ptrval = s.getMemory().alloc(sizeexpr, align, blkkind, glbvar_bid);
  } else {
    ptrval = s.getMemory().alloc(sizeexpr, align, blkkind, nullopt,
                                 &glbvar_bid);
    s.addGlobalVarBid(getName(), glbvar_bid);
  }
  return { move(ptrval), true };
}


Input::Input(Type &type, string &&name)
  : Value(type, string(name)), smt_name(move(name)) {}

void Input::copySMTName(const Input &other) {
  smt_name = other.smt_name;
}

void Input::print(ostream &os) const {
  UNREACHABLE();
}

StateValue Input::toSMT(State &s) const {
  // 00: normal, 01: undef, else: poison
  expr type = getTyVar();

  auto val = getType().mkInput(s, smt_name.c_str());

  if (!config::disable_undef_input) {
    auto [undef, vars] = getType().mkUndefInput(s);
    for (auto &v : vars) {
      s.addUndefVar(move(v));
    }
    val = expr::mkIf(type.extract(0, 0) == 0, val, undef);
  }

  expr poison = getType().getDummyValue(false).non_poison;
  expr non_poison = getType().getDummyValue(true).non_poison;

  return { move(val),
           config::disable_poison_input
             ? move(non_poison)
             : expr::mkIf(type.extract(1, 1) == 0, non_poison, poison) };
}

expr Input::getTyVar() const {
  string tyname = "ty_" + smt_name;
  return expr::mkVar(tyname.c_str(), 2);
}

}
