// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include "smt/expr.h"
#include "util/compiler.h"

using namespace smt;
using namespace util;
using namespace std;

static unsigned gbl_fresh_id = 0;

namespace IR {

void Value::reset_gbl_id() {
  gbl_fresh_id = 0;
}

string Value::fresh_id() {
  return to_string(++gbl_fresh_id);
}

Value::Value(unique_ptr<Type> &&type, string &&name, bool mk_unique_name)
  : type(move(type)), name(move(name)) {
  if (mk_unique_name)
    this->type->setName(getName() + '_' + fresh_id());
  else
    this->type->setName(getName());
}

void Value::fixupTypes(const Model &m) {
  type->fixup(m);
}

Value::~Value() {}

ostream& operator<<(ostream &os, const Value &val) {
  auto t = val.getType().toString();
  if (!t.empty())
    os << t << ' ';
  return os << val.getName();
}


IntConst::IntConst(unique_ptr<Type> &&type, int64_t val)
  : Value(move(type), to_string(val), true), val(val) {
  getWType().enforceIntType();
}

void IntConst::print(ostream &os) const {
  UNREACHABLE();
}

StateValue IntConst::toSMT(State &s) const {
  return { expr::mkInt(val, bits()), true };
}

expr IntConst::getTypeConstraints() const {
  unsigned min_bits = (val >= 0 ? 63 : 64) - num_sign_bits(val);
  return getType().getTypeConstraints() &&
         getType().sizeVar().uge(min_bits);
}


UndefValue::UndefValue(unique_ptr<Type> &&type)
  : Value(std::move(type), "undef", true) {}

void UndefValue::print(ostream &os) const {
  os << "undef";
}

StateValue UndefValue::toSMT(State &s) const {
  auto name = getFreshName();
  expr var = expr::mkVar(name.c_str(), bits());
  s.addUndefVar(var);
  return { move(var), true };
}

expr UndefValue::getTypeConstraints() const {
  return getType().getTypeConstraints();
}

string UndefValue::getFreshName() {
  return "undef_" + fresh_id();
}


PoisonValue::PoisonValue(unique_ptr<Type> &&type)
  : Value(std::move(type), "poison", true) {}

void PoisonValue::print(ostream &os) const {
  os << "poison";
}

StateValue PoisonValue::toSMT(State &s) const {
  return { {}, false };
}

expr PoisonValue::getTypeConstraints() const {
  return getType().getTypeConstraints();
}


void Input::print(std::ostream &os) const {
  os << getName();
}

StateValue Input::toSMT(State &s) const {
  // 00: normal, 01: undef, else: poison
  string tyname = "ty_" + getName();
  expr type = expr::mkVar(tyname.c_str(), 2);

  auto bw = bits();
  string uname = UndefValue::getFreshName();
  expr undef = expr::mkVar(uname.c_str(), bw);
  s.addUndefVar(undef);

  return { expr::mkIf(type == expr::mkUInt(0, 2),
                      expr::mkVar(getName().c_str(), bw),
                      move(undef)),
           type.extract(1,1) == expr::mkUInt(0, 1) };
}

expr Input::getTypeConstraints() const {
  return getType().getTypeConstraints();
}

}
