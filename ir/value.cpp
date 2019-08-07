// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include "util/config.h"
#include <sstream>

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
  if (!dynamic_cast<VoidType*>(&val.getType())) {
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
  expr var = expr::mkVar(name.c_str(), getType().getDummyValue());
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
  return { getType().getDummyValue(), false };
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
  // 00: normal, 01: undef, else: poison
  expr type = getTyVar();

  auto [val, vars] = getType().mkInput(s, getName().c_str());

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

  return { move(val),
           config::disable_poison_input ? true : type.extract(1, 1) == 0 };
}

expr Input::getTyVar() const {
  string tyname = "ty_" + getName();
  return expr::mkVar(tyname.c_str(), 2);
}

static string genVectorName (std::vector<Value *> vals) {
  ostringstream ss;
  ss << "<";
  for (unsigned idx = 0 ; idx < vals.size(); idx++) {
    ss << *vals[idx];
    if (idx != vals.size() -1)
      ss << ", ";
  }
  ss << ">";
  return ss.str();
}
VectorValue::VectorValue(Type &type, std::vector<Value *> vals)
  : Value(type, genVectorName(vals)), vals(std::move(vals)) {}

void VectorValue::print(ostream &os) const {
  os << "TODO";
}

StateValue VectorValue::toSMT(State &s) const {
  return { expr() , true };
}

}
