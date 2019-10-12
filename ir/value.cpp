// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include "util/config.h"

using namespace smt;
using namespace std;
using namespace util;

static unsigned gbl_fresh_id = 0;

namespace IR {

VoidValue Value::voidVal;

void Value::reset_gbl_id() {
  gbl_fresh_id = 0;
}

string Value::fresh_id() {
  return to_string(++gbl_fresh_id);
}

expr Value::getTypeConstraints() const {
  return getType().getTypeConstraints();
}

void Value::fixupTypes(const Model &m) {
  type.fixup(m);
}

ostream& operator<<(ostream &os, const Value &val) {
  auto t = val.getType().toString();
  os << t;
  if (&val.getType() != &Type::voidTy) {
    if (!t.empty()) os << ' ';
    os << val.getName();
  }
  return os;
}


void UndefValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue UndefValue::toSMT(State &s) const {
  auto name = getFreshName();
  auto val = getType().getDummyValue(true);
  expr var = expr::mkVar(name.c_str(), val.value);
  s.addUndefVar(var);
  return { move(var), move(val.non_poison) };
}

string UndefValue::getFreshName() {
  return "undef_" + fresh_id();
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
  UNREACHABLE();
}

StateValue GlobalVariable::toSMT(State &s) const {
  auto sizeexpr = expr::mkInt(allocsize, 64);
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
  if (initval) {
    assert(isconst);
    s.getMemory().store(ptrval, s[*initval], initval->getType(), align);
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

  auto [val, vars] = getType().mkInput(s, smt_name.c_str());

  if (!config::disable_undef_input) {
    vector<pair<expr, expr>> repls;

    for (auto &v : vars) {
      string uname = UndefValue::getFreshName();
      expr undef = expr::mkVar(uname.c_str(), v);
      s.addUndefVar(undef);
      repls.emplace_back(v, move(undef));
    }

    val = expr::mkIf(type.extract(0, 0) == 0, val, val.subst(repls));
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
