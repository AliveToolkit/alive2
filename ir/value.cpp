// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include "smt/expr.h"
#include "util/compiler.h"

using namespace smt;
using namespace util;
using namespace std;

static unsigned gbl_fresh_id = 0;
static string fresh_id() {
  return to_string(++gbl_fresh_id);
}

namespace IR {

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
         getType().atLeastBits(min_bits);
}


void Input::print(std::ostream &os) const {
  os << getName();
}

StateValue Input::toSMT(State &s) const {
  string pname = getName() + "_np";
  auto poison = expr::mkBoolVar(pname.c_str());
  auto value = expr::mkVar(getName().c_str(), getType().bits());
  s.addQuantVar(poison);
  s.addQuantVar(value);
  return { move(value), move(poison) };
}

expr Input::getTypeConstraints() const {
  return getType().getTypeConstraints();
}

}
