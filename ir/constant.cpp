// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/constant.h"
#include "smt/expr.h"
#include "util/compiler.h"
#include <bit>
#include <cassert>
#include <cmath>
// TODO: remove cstring when migrated to std::bit_cast
#include <cstring>
#include <iomanip>
#include <sstream>

using namespace smt;
using namespace std;
using namespace util;

namespace IR {

void Constant::print(ostream &os) const {
  UNREACHABLE();
}


IntConst::IntConst(Type &type, int64_t val)
  : Constant(type, to_string(val)), val(val) {}

IntConst::IntConst(Type &type, string &&val)
  : Constant(type, string(val)), val(std::move(val)) {}

StateValue IntConst::toSMT(State &s) const {
  if (auto v = get_if<int64_t>(&val))
    return { expr::mkInt(*v, bits()), true };
  return { expr::mkInt(get<string>(val).c_str(), bits()), true };
}

expr IntConst::getTypeConstraints() const {
  unsigned min_bits = 0;
  if (auto v = get_if<int64_t>(&val))
    min_bits = (*v >= 0 ? 63 : 64) - num_sign_bits(*v);

  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         getType().sizeVar().uge(min_bits);
}

template <typename TO, typename TI>
static TO mbit_cast(TI bits) {
  // FIXME: Apple's clang doesn't have std::bit_cast nor does gcc 10
  TO fp;
  assert(sizeof(fp) == sizeof(bits));
  memcpy(&fp, &bits, sizeof(fp));
  return fp;
}

static string to_hex(Type &type, const string &val) {
  uint64_t num = strtoull(val.c_str(), nullptr, 10);
  ostringstream os;
  os << "0x" << hex << setfill('0') << setw(type.bits()/4) << num;
  return std::move(os).str();
}

template <typename TO, typename TI>
static string bits_to_float(Type &type, const string &val) {
  uint64_t num = strtoull(val.c_str(), nullptr, 10);
  TO fp = mbit_cast<TO, TI>((TI)num);
  return isnan(fp) ? to_hex(type, val) : to_string(fp);
}

static string int_to_readable_float(Type &type, const string &val) {
  switch (type.getAsFloatType()->getFpType()) {
  case FloatType::Float:   return bits_to_float<float, unsigned>(type, val);
  case FloatType::Double:  return bits_to_float<double, uint64_t>(type, val);
  case FloatType::Quad:    return val;
  case FloatType::Half:
  case FloatType::BFloat:  return to_hex(type, val);
  case FloatType::Unknown: UNREACHABLE();
  }
  UNREACHABLE();
}

FloatConst::FloatConst(Type &type, string val, bool bit_value)
  : Constant(type, bit_value ? int_to_readable_float(type, val) : val),
  val(std::move(val)), bit_value(bit_value) {}

expr FloatConst::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         getType().enforceFloatType();
}

StateValue FloatConst::toSMT(State &s) const {
  if (bit_value)
    return { expr::mkNumber(val.c_str(), expr::mkUInt(0, getType().bits())),
             true };

  return { expr::mkNumber(val.c_str(),
                          getType().getAsFloatType()->getDummyFloat())
             .float2BV(),
           true };
}


StateValue ConstantInput::toSMT(State &s) const {
  auto type = getType().getDummyValue(false).value;
  return { expr::mkVar(getName().c_str(), type), true };
}

expr ConstantInput::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         (getType().enforceIntType() || getType().enforceFloatType());
}


ConstantBinOp::ConstantBinOp(Type &type, Constant &lhs, Constant &rhs, Op op)
  : Constant(type, ""), lhs(lhs), rhs(rhs), op(op) {
  const char *opname = nullptr;
  switch (op) {
  case ADD:  opname = " + "; break;
  case SUB:  opname = " - "; break;
  case SDIV: opname = " / "; break;
  case UDIV: opname = " /u "; break;
  }

  string str = '(' + this->lhs.getName();
  str += opname;
  str += rhs.getName();
  str += ')';
  this->setName(std::move(str));
}

static void div_ub(const expr &a, const expr &b, State &s, bool sign) {
  s.addPre(b != 0);
  if (sign)
    s.addPre(a != expr::IntSMin(b.bits()) || b != -1);
}

StateValue ConstantBinOp::toSMT(State &s) const {
  auto &[a, ap] = s[lhs];
  auto &[b, bp] = s[rhs];
  expr val;

  switch (op) {
  case ADD: val = a + b; break;
  case SUB: val = a - b; break;
  case SDIV:
    val = a.sdiv(b);
    div_ub(a, b, s, true);
    break;
  case UDIV:
    val = a.udiv(b);
    div_ub(a, b, s, false);
    break;
  }
  return { std::move(val), ap && bp };
}

expr ConstantBinOp::getTypeConstraints() const {
  return Value::getTypeConstraints() &&
         getType().enforceIntType() &&
         getType() == lhs.getType() &&
         getType() == rhs.getType();
}


ConstantFn::ConstantFn(Type &type, string_view name, vector<Value*> &&args)
  : Constant(type, ""), args(std::move(args)) {
  unsigned num_args;
  if (name == "log2") {
    fn = LOG2;
    num_args = 1;
  } else if (name == "width") {
    fn = WIDTH;
    num_args = 1;
  } else {
    throw ConstantFnException("Unknown function: " + string(name));
  }

  auto actual_args = this->args.size();
  if (actual_args != num_args)
    throw ConstantFnException("Expected " + to_string(num_args) +
                              " parameters for " + string(name) + ", but got " +
                              to_string(actual_args));

  string str = string(name) + '(';
  bool first = true;
  for (auto arg : this->args) {
    if (!first)
      str += ", ";
    first = false;
    str += arg->getName();
  }
  str += ')';
  this->setName(std::move(str));
}

StateValue ConstantFn::toSMT(State &s) const {
  expr r;
  switch (fn) {
  case LOG2: {
    auto &[v, vp] = s[*args[0]];
    return { v.log2(bits()), expr(vp) };
  }
  case WIDTH:
    r = args[0]->bits();
    break;
  }
  return { std::move(r), true };
}

expr ConstantFn::getTypeConstraints() const {
  expr r = Value::getTypeConstraints();
  for (auto a : args) {
    r &= a->getTypeConstraints();
  }

  Type &ty = getType();
  switch (fn) {
  case LOG2:
  case WIDTH:
    r &= ty.enforceIntType();
    break;
  }
  return r;
}

optional<int64_t> getInt(const Value &val) {
  if (auto i = dynamic_cast<const IntConst*>(&val)) {
    if (auto n = i->getInt())
      return *n;
  }
  return {};
}

uint64_t getIntOr(const Value &val, uint64_t default_value) {
  if (auto n = getInt(val))
    return *n;
  return default_value;
}
}
