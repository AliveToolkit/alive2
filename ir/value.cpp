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
  if (!t.empty())
    os << t;
  if (!dynamic_cast<VoidType*>(&val.getType()))
    os << ' ' << val.getName();
  return os;
}


void UndefValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue UndefValue::toSMT(State &s) const {
  auto name = getFreshName();
  expr var = expr::mkVar(name.c_str(), bits());
  s.addUndefVar(var);
  return { move(var), true };
}

string UndefValue::getFreshName() {
  return "undef_" + fresh_id();
}


void PoisonValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue PoisonValue::toSMT(State &s) const {
  return { expr::mkUInt(0, bits()), false };
}


void VoidValue::print(ostream &os) const {
  UNREACHABLE();
}

StateValue VoidValue::toSMT(State &s) const {
  return { false, false };
}


void Input::print(std::ostream &os) const {
  UNREACHABLE();
}

StateValue Input::toSMT(State &s) const {
  if (auto fpType = getType().getAsFloatType()) {
    FloatType::FpType fpTy = fpType->getFpType();
    switch (fpTy) {
    case FloatType::Float:
      return {expr::mkFloatVar(getName().c_str()), true};
    case FloatType::Double:
      return {expr::mkDoubleVar(getName().c_str()), true};
    default:
      // TODO: support more fp types
      UNREACHABLE();
    }
  }

  // 00: normal, 01: undef, else: poison
  expr type = getTyVar();

  auto bw = bits();
  expr val;
  if (config::disable_undef_input) {
    val = expr::mkVar(getName().c_str(), bw);
  } else {
    string uname = UndefValue::getFreshName();
    expr undef = expr::mkVar(uname.c_str(), bw);
    s.addUndefVar(undef);
    val = expr::mkIf(type == expr::mkUInt(0, 2),
                     expr::mkVar(getName().c_str(), bw),
                     move(undef));
  }

  return { move(val),
           config::disable_poison_input
             ? true
             : type.extract(1,1) == expr::mkUInt(0, 1) };
}

expr Input::getTyVar() const {
  string tyname = "ty_" + getName();
  return expr::mkVar(tyname.c_str(), 2);
}

}
