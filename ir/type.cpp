// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/type.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <cassert>
#include <sstream>

using namespace smt;
using namespace std;

static constexpr unsigned var_type_bits = 3;
static constexpr unsigned var_bw_bits = 10;


namespace IR {

expr Type::var(const char *var, unsigned bits) const {
  auto str = name + '_' + var;
  return expr::mkVar(str.c_str(), bits);
}

expr Type::typeVar() const {
  return var("type", var_type_bits);
}

expr Type::sizeVar() const {
  return var("bw", var_bw_bits);
}

expr Type::is(unsigned t) const {
  return typeVar() == expr::mkUInt(t, var_type_bits);
}

expr Type::isInt() const    { return is(SymbolicType::Int); }
expr Type::isFloat() const  { return is(SymbolicType::Float); }
expr Type::isPtr() const    { return is(SymbolicType::Ptr); }
expr Type::isArray() const  { return is(SymbolicType::Array); }
expr Type::isVector() const { return is(SymbolicType::Vector); }

unsigned Type::bits() const {
  UNREACHABLE();
}

expr Type::operator==(const Type &b) const {
  if (this == &b)
    return true;

#define CMP(Ty)                                                                \
  if (auto lhs = dynamic_cast<const Ty*>(this)) {                              \
    if (auto rhs = dynamic_cast<const Ty*>(&b))                                \
      return *lhs == *rhs;                                                     \
    if (auto rhs = dynamic_cast<const SymbolicType*>(&b))                      \
      return *rhs == *lhs;                                                     \
    return false;                                                              \
  }

  CMP(IntType)
  CMP(FloatType)
  CMP(PtrType)
  CMP(ArrayType)
  CMP(VectorType)

#undef CMP

  if (auto lhs = dynamic_cast<const SymbolicType*>(this)) {
    return *lhs == b;
  }

  return false;
}

expr Type::enforceIntType() const {
  return false;
}

expr Type::enforceIntOrPtrOrVectorType() const {
  return false;
}

string Type::toString() const {
  stringstream s;
  print(s);
  return s.str();
}

Type::~Type() {}


expr VoidType::getTypeConstraints() const {
  return true;
}

void VoidType::fixup(const Model &m) {
  // do nothing
}

void VoidType::print(ostream &os) const {
  os << "void";
}


unsigned IntType::bits() const {
  return bitwidth;
}

expr IntType::getTypeConstraints() const {
  // limit ints to be between 1 and 64 bits
  // TODO: lift 64-bit restriction
  auto bw = sizeVar();
  return bw != expr::mkUInt(0, var_bw_bits) &&
         bw.ule(expr::mkUInt(64, var_bw_bits));
}

expr IntType::sizeVar() const {
  return defined ? expr::mkUInt(bits(), var_bw_bits) : Type::sizeVar();
}

expr IntType::operator==(const IntType &rhs) const {
  return sizeVar() == rhs.sizeVar();
}

void IntType::fixup(const Model &m) {
  if (!defined)
    bitwidth = m.getUInt(sizeVar());
}

expr IntType::enforceIntType() const {
  return true;
}

expr IntType::enforceIntOrPtrOrVectorType() const {
  return true;
}

void IntType::print(ostream &os) const {
  if (defined)
    os << 'i' << bits();
}


expr FloatType::getTypeConstraints() const {
  // TODO
  return false;
}

expr FloatType::operator==(const FloatType &rhs) const {
  // TODO
  return false;
}

void FloatType::fixup(const Model &m) {
  // TODO
}

void FloatType::print(ostream &os) const {
  os << "TODO";
}


expr PtrType::getTypeConstraints() const {
  // TODO
  return false;
}

expr PtrType::operator==(const PtrType &rhs) const {
  // TODO
  return sizeVar() == rhs.sizeVar();
}

void PtrType::fixup(const Model &m) {
  // TODO
}

expr PtrType::enforceIntOrPtrOrVectorType() const {
  return true;
}

void PtrType::print(ostream &os) const {
  os << "TODO";
}


expr ArrayType::getTypeConstraints() const {
  // TODO
  return false;
}

expr ArrayType::operator==(const ArrayType &rhs) const {
  // TODO
  return false;
}

void ArrayType::fixup(const Model &m) {
  // TODO
}

void ArrayType::print(ostream &os) const {
  os << "TODO";
}


expr VectorType::getTypeConstraints() const {
  // TODO
  return false;
}

expr VectorType::operator==(const VectorType &rhs) const {
  // TODO
  return false;
}

void VectorType::fixup(const Model &m) {
  // TODO
}

expr VectorType::enforceIntOrPtrOrVectorType() const {
  return true;
}

void VectorType::print(ostream &os) const {
  os << "TODO";
}


SymbolicType::SymbolicType(string &&name)
  : Type(string(name)), i(string(name)), f(string(name)), p(string(name)),
    a(string(name)), v(string(name)) {}

unsigned SymbolicType::bits() const {
  switch (typ) {
  case Int:    return i.bits();
  case Float:  return f.bits();
  case Ptr:    return p.bits();
  case Array:  return a.bits();
  case Vector: return v.bits();
  case Undefined:
    assert(0 && "undefined at SymbolicType::bits()");
  }
  UNREACHABLE();
}

expr SymbolicType::getTypeConstraints() const {
  expr c(false);
  c |= isInt()    && i.getTypeConstraints();
  c |= isFloat()  && f.getTypeConstraints();
  c |= isPtr()    && p.getTypeConstraints();
  c |= isArray()  && a.getTypeConstraints();
  c |= isVector() && v.getTypeConstraints();
  return c;
}

expr SymbolicType::operator==(const Type &b) const {
  if (this == &b)
    return true;

  if (auto rhs = dynamic_cast<const IntType*>(&b))
    return isInt() && i == *rhs;
  if (auto rhs = dynamic_cast<const FloatType*>(&b))
    return isFloat() && f == *rhs;
  if (auto rhs = dynamic_cast<const PtrType*>(&b))
    return isPtr() && p == *rhs;
  if (auto rhs = dynamic_cast<const ArrayType*>(&b))
    return isArray() && a == *rhs;
  if (auto rhs = dynamic_cast<const VectorType*>(&b))
    return isVector() && v == *rhs;

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {  
    expr c(false);
    c |= isInt()    && i == rhs->i;
    c |= isFloat()  && f == rhs->f;
    c |= isPtr()    && p == rhs->p;
    c |= isArray()  && a == rhs->a;
    c |= isVector() && v == rhs->v;
    return move(c) && typeVar() == rhs->typeVar();
  }
  assert(0 && "unhandled case in SymbolicType::operator==");
  UNREACHABLE();
}

void SymbolicType::fixup(const Model &m) {
  unsigned smt_typ = m.getUInt(typeVar());
  assert(smt_typ >= Int && smt_typ <= Vector);
  typ = TypeNum(smt_typ);

  switch (typ) {
  case Int:    i.fixup(m); break;
  case Float:  f.fixup(m); break;
  case Ptr:    p.fixup(m); break;
  case Array:  a.fixup(m); break;
  case Vector: v.fixup(m); break;
  case Undefined:
    UNREACHABLE();
  }
}

expr SymbolicType::enforceIntType() const {
  return isInt();
}

expr SymbolicType::enforceIntOrPtrOrVectorType() const {
  return isInt() || isPtr() || isVector();
}

void SymbolicType::print(ostream &os) const {
  if (!name.empty()) {
    os << name;
    return;
  }

  switch (typ) {
  case Int:       i.print(os); break;
  case Float:     f.print(os); break;
  case Ptr:       p.print(os); break;
  case Array:     a.print(os); break;
  case Vector:    v.print(os); break;
  case Undefined: break;
  }
}

}
