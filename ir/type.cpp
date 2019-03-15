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
static constexpr unsigned var_bw_bits = 8;


namespace IR {

VoidType Type::voidTy;

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
expr Type::isStructure() const { return is(SymbolicType::Structure); }

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

expr Type::enforceIntOrVectorType() const {
  return false;
}

expr Type::enforceIntOrPtrOrVectorType() const {
  return false;
}

expr Type::enforceStructureType() const {
  return false;
}

ostream& operator<<(ostream &os, const Type &t) {
  t.print(os);
  return os;
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
  // since size cannot be unbounded, limit it between 1 and 64 bits if undefined
  auto bw = sizeVar();
  auto r = bw != expr::mkUInt(0, var_bw_bits);
  if (!defined)
    r &= bw.ule(expr::mkUInt(64, var_bw_bits));
  return r;
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

expr IntType::enforceIntOrVectorType() const {
  return true;
}

expr IntType::enforceIntOrPtrOrVectorType() const {
  return true;
}

void IntType::print(ostream &os) const {
  if (bits())
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

expr PtrType::enforceIntOrVectorType() const {
  return false;
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

expr VectorType::enforceIntOrVectorType() const {
  return true;
}

expr VectorType::enforceIntOrPtrOrVectorType() const {
  return true;
}

void VectorType::print(ostream &os) const {
  os << "TODO";
}

expr StructureType::getTypeConstraints() const {
  expr res(true);
  for (auto type : children) {
    res &= type->enforceIntType();
  }

  return res;
}

expr StructureType::operator==(const StructureType &rhs) const {
  if (children.size() != rhs.children.size())
    return false;

  expr res(false);
  for (unsigned i = 0; i < children.size(); i++) {
    res = res || *children[i] == *rhs.children[i];
  }

  return res;
}

expr StructureType::enforceStructureType() const {
  return !children.empty();
}

void StructureType::fixup(const Model &m) {
  // TODO
}

expr StructureType::enforceIntOrPtrOrVectorType() const {
  return false;
}

unsigned StructureType::bits() const {
  unsigned res = 0;
  for (unsigned i = 0; i < children.size(); i++) {
    res += children[i]->bits();
  }

  return res;
}

void StructureType::print(ostream &os) const {
  assert(children.size() > 0);
  os << '{';
  children[0]->print(os);
  for (unsigned i = 1; i < children.size(); i++) {
    os << ", ";
    children[i]->print(os);
  }
  os << '}';
}

Type& StructureType::getChildType(unsigned index) const {
  assert(index < children.size());
  return *children[index];
}

expr StructureType::extract(const expr &struct_val, unsigned index) const {
  unsigned total_till_now = 0;
  assert(index < children.size());
  for (unsigned i = 0; i <= index; i++) {
    total_till_now += children[i]->bits();
  }
  unsigned low = struct_val.bits() - total_till_now;
  return struct_val.extract(low + children[index]->bits() - 1, low);
}


SymbolicType::SymbolicType(string &&name, bool named)
  : Type(string(name)), i(string(name)), f(string(name)), p(string(name)),
    a(string(name)), v(string(name)), st(string(name)), named(named) {}

unsigned SymbolicType::bits() const {
  switch (typ) {
  case Int:    return i.bits();
  case Float:  return f.bits();
  case Ptr:    return p.bits();
  case Array:  return a.bits();
  case Vector: return v.bits();
  case Structure: return st.bits();
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
  if (auto rhs = dynamic_cast<const StructureType*>(&b))
    return isStructure() && st == *rhs;

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {
    expr c(false);
    c |= isInt()    && i == rhs->i;
    c |= isFloat()  && f == rhs->f;
    c |= isPtr()    && p == rhs->p;
    c |= isArray()  && a == rhs->a;
    c |= isVector() && v == rhs->v;
    c |= isStructure() && st == rhs->st;
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
  case Structure: st.fixup(m); break;
  case Undefined:
    UNREACHABLE();
  }
}

expr SymbolicType::enforceIntType() const {
  return isInt();
}

expr SymbolicType::enforceIntOrVectorType() const {
  return isInt() || isVector();
}

expr SymbolicType::enforceIntOrPtrOrVectorType() const {
  return isInt() || isPtr() || isVector();
}

expr SymbolicType::enforceStructureType() const {
  return isStructure();
}

void SymbolicType::print(ostream &os) const {
  if (named) {
    os << name;
    return;
  }

  switch (typ) {
  case Int:       i.print(os); break;
  case Float:     f.print(os); break;
  case Ptr:       p.print(os); break;
  case Array:     a.print(os); break;
  case Vector:    v.print(os); break;
  case Structure: st.print(os); break;
  case Undefined: break;
  }
}

}
