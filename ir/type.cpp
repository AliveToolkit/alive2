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
expr Type::isStruct() const { return is(SymbolicType::Struct); }

expr Type::operator==(const Type &b) const {
  if (this == &b)
    return true;

#define CMP(Ty)                                                                \
  if (auto lhs = dynamic_cast<const Ty*>(this)) {                              \
    if (auto rhs = dynamic_cast<const Ty*>(&b))                                \
      return *lhs == *rhs;                                                     \
  }

  CMP(IntType)
  CMP(FloatType)
  CMP(PtrType)
  CMP(ArrayType)
  CMP(VectorType)
  CMP(StructType)
#undef CMP

  if (auto lhs = dynamic_cast<const SymbolicType*>(this))
    return *lhs == b;

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b))
    return *rhs == *this;

  return false;
}

expr Type::sameType(const Type &b) const {
  if (this == &b)
    return true;

#define CMP(Ty)                                                                \
  if (auto lhs = dynamic_cast<const Ty*>(this)) {                              \
    if (auto rhs = dynamic_cast<const Ty*>(&b))                                \
      return lhs->sameType(*rhs);                                              \
  }

  CMP(IntType)
  CMP(FloatType)
  CMP(PtrType)
  CMP(ArrayType)
  CMP(VectorType)
  CMP(StructType)
#undef CMP

  if (auto lhs = dynamic_cast<const SymbolicType*>(this))
    return lhs->sameType(b);

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b))
    return rhs->sameType(*this);

  return false;
}

expr Type::enforceIntType(unsigned bits) const {
  return false;
}

expr Type::enforceIntOrVectorType() const {
  return false;
}

expr Type::enforceIntOrPtrOrVectorType() const {
  return false;
}

expr Type::enforcePtrType() const {
  return false;
}

expr Type::enforceStructType() const {
  return false;
}

expr Type::enforceAggregateType() const {
  return false;
}

expr Type::enforceFloatType() const {
  return false;
}

const StructType* Type::getAsStructType() const {
  UNREACHABLE();
}

const FloatType* Type::getAsFloatType() const {
  return nullptr;
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


unsigned VoidType::bits() const {
  UNREACHABLE();
}

expr VoidType::getDummyValue() const {
  UNREACHABLE();
}

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

expr IntType::getDummyValue() const {
  return expr::mkUInt(0, bits());
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

expr IntType::sameType(const IntType &rhs) const {
  return true;
}

void IntType::fixup(const Model &m) {
  if (!defined)
    bitwidth = m.getUInt(sizeVar());
}

expr IntType::enforceIntType(unsigned bits) const {
  return bits ? sizeVar() == bits : true;
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


unsigned FloatType::bits() const {
  switch (fpType) {
  case FpType::Quarter:
    return 8;
  case FpType::Half:
    return 16;
  case FpType::BFloat16:
    return 16;
  case FpType::Float:
    return 32;
  case FpType::Double:
    return 64;
  case FpType::IEEE80:
    return 80;
  case FpType::FP128:
    return 128;
  }
  UNREACHABLE();
}

const FloatType* FloatType::getAsFloatType() const {
  return this;
}

expr FloatType::sizeVar() const {
  return defined ? expr::mkUInt(getFpType(), var_bw_bits) : Type::sizeVar();
}

expr FloatType::getDummyValue() const {
  // TODO
  return {};
}

expr FloatType::getTypeConstraints() const {
  if (defined)
    return true;

  auto bw = sizeVar();
  // TODO: support more fp types
  auto isFloat = bw == expr::mkUInt(Float, var_bw_bits);
  auto isDouble = bw == expr::mkUInt(Double, var_bw_bits);
  return (isFloat || isDouble);
}

expr FloatType::operator==(const FloatType &rhs) const {
  return sizeVar() == rhs.sizeVar();
}

expr FloatType::sameType(const FloatType &rhs) const {
  return true;
}

void FloatType::fixup(const Model &m) {
  if (defined)
    return;

  unsigned fp_typ = m.getUInt(sizeVar());
  assert(fp_typ >= Quarter && fp_typ <= FP128);
  fpType = FpType(fp_typ);
}

void FloatType::print(ostream &os) const {
  switch (fpType) {
  case FpType::Quarter:
    os << "quarter";
    break;
  case FpType::Half:
    os << "half";
    break;
  case FpType::BFloat16:
    os << "bfloat16";
    break;
  case FpType::Float:
    os << "float";
    break;
  case FpType::Double:
    os << "double";
    break;
  case FpType::IEEE80:
    os << "ieee80";
    break;
  case FpType::FP128:
    os << "fp128";
    break;
  }
}

expr FloatType::enforceFloatType() const {
  return true;
}

PtrType::PtrType(unsigned addr_space)
  : Type(addr_space == 0 ? "*" : "as(" + to_string(addr_space) + ")*"),
    addr_space(addr_space), defined(true) {}

expr PtrType::ASVar() const {
  return defined ? expr::mkUInt(addr_space, 2) : var("as", 2);
}

unsigned PtrType::bits() const {
  // TODO: make this configurable
  return 48;
}

expr PtrType::getDummyValue() const {
  return expr::mkUInt(0, bits());
}

expr PtrType::getTypeConstraints() const {
  return sizeVar() == bits();
}

expr PtrType::operator==(const PtrType &rhs) const {
  return sizeVar() == rhs.sizeVar() &&
         ASVar() == rhs.ASVar();
}

expr PtrType::sameType(const PtrType &rhs) const {
  return *this == rhs;
}

void PtrType::fixup(const Model &m) {
  if (!defined)
    addr_space = m.getUInt(ASVar());
}

expr PtrType::enforceIntOrVectorType() const {
  return false;
}

expr PtrType::enforceIntOrPtrOrVectorType() const {
  return true;
}

expr PtrType::enforcePtrType() const {
  return true;
}

void PtrType::print(ostream &os) const {
  if (addr_space != 0)
    os << "as(" << addr_space << ")*";
  else
    os << '*';
}


unsigned ArrayType::bits() const {
  // TODO
  return 0;
}

expr ArrayType::getDummyValue() const {
  // TODO
  return {};
}

expr ArrayType::getTypeConstraints() const {
  // TODO
  return false;
}

expr ArrayType::operator==(const ArrayType &rhs) const {
  // TODO
  return false;
}

expr ArrayType::sameType(const ArrayType &rhs) const {
  // TODO
  return false;
}

void ArrayType::fixup(const Model &m) {
  // TODO
}

expr ArrayType::enforceAggregateType() const {
  return true;
}

void ArrayType::print(ostream &os) const {
  os << "TODO";
}


unsigned VectorType::bits() const {
  // TODO
  return 0;
}

expr VectorType::getDummyValue() const {
  // TODO
  return {};
}

expr VectorType::getTypeConstraints() const {
  // TODO
  return false;
}

expr VectorType::operator==(const VectorType &rhs) const {
  // TODO
  return false;
}

expr VectorType::sameType(const VectorType &rhs) const {
  // TODO
  return false;
}

void VectorType::fixup(const Model &m) {
  // TODO
}

expr VectorType::enforceIntOrVectorType() const {
  // TODO: check if elements are int
  return false;
}

expr VectorType::enforceIntOrPtrOrVectorType() const {
  // TODO: check if elements are int/ptr
  return false;
}

void VectorType::print(ostream &os) const {
  os << "TODO";
}


unsigned StructType::bits() const {
  unsigned res = 0;
  for (auto c : children) {
    res += c->bits();
  }
  return res;
}

expr StructType::getDummyValue() const {
  if (children.empty())
    return expr::mkUInt(0, 1);

  expr res;
  bool first = true;
  // TODO: what if a function returns a struct with no children?
  for (auto c : children) {
    if (first) {
      res = c->getDummyValue();
      first = false;
    } else {
      res = res.concat(c->getDummyValue());
    }
  }
  return res;
}

expr StructType::getTypeConstraints() const {
  expr res(true);
  for (auto c : children) {
    res &= c->getTypeConstraints();
  }
  return res;
}

expr StructType::operator==(const StructType &rhs) const {
  if (children.size() != rhs.children.size())
    return false;

  expr res(true);
  for (unsigned i = 0, e = children.size(); i != e; ++i) {
    res &= *children[i] == *rhs.children[i];
  }
  return res;
}

expr StructType::sameType(const StructType &rhs) const {
  if (children.size() != rhs.children.size())
    return false;

  expr res(true);
  for (unsigned i = 0, e = children.size(); i != e; ++i) {
    res &= children[i]->sameType(*rhs.children[i]);
  }
  return res;
}

void StructType::fixup(const Model &m) {
  for (auto c : children) {
    c->fixup(m);
  }
}

expr StructType::enforceStructType() const {
  return true;
}

expr StructType::enforceAggregateType() const {
  return true;
}

const StructType* StructType::getAsStructType() const {
  return this;
}

void StructType::print(ostream &os) const {
  os << '{';
  bool first = true;
  for (auto c : children) {
    if (!first)
      os << ", ";
    first = false;
    c->print(os);
  }
  os << '}';
}

smt::expr StructType::numElements() const {
  // TODO: fix symbolic struct type
  return expr::mkUInt(children.size(), 8);
}

Type& StructType::getChild(unsigned index) const {
  // TODO: fix symbolic struct type
  return *children[index];
}

expr StructType::extract(const expr &struct_val, unsigned index) const {
  unsigned total_till_now = 0;
  assert(index < children.size());
  for (unsigned i = 0; i <= index; ++i) {
    total_till_now += children[i]->bits();
  }
  unsigned low = struct_val.bits() - total_till_now;
  return struct_val.extract(low + children[index]->bits() - 1, low);
}


SymbolicType::SymbolicType(string &&name, bool named)
  : Type(string(name)), i(string(name)), f(string(name)), p(string(name)),
    a(string(name)), v(string(name)), s(string(name)), named(named) {}

unsigned SymbolicType::bits() const {
  switch (typ) {
  case Int:    return i.bits();
  case Float:  return f.bits();
  case Ptr:    return p.bits();
  case Array:  return a.bits();
  case Vector: return v.bits();
  case Struct: return s.bits();
  case Undefined:
    assert(0 && "undefined at SymbolicType::bits()");
  }
  UNREACHABLE();
}

expr SymbolicType::getDummyValue() const {
  switch (typ) {
  case Int:    return i.getDummyValue();
  case Float:  return f.getDummyValue();
  case Ptr:    return p.getDummyValue();
  case Array:  return a.getDummyValue();
  case Vector: return v.getDummyValue();
  case Struct: return s.getDummyValue();
  case Undefined:
    break;
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
  c |= isStruct() && s.getTypeConstraints();
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
  if (auto rhs = dynamic_cast<const StructType*>(&b))
    return isStruct() && s == *rhs;

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {
    expr c(false);
    c |= isInt()    && i == rhs->i;
    c |= isFloat()  && f == rhs->f;
    // TODO: to reenable this, we need partial models and/or a shrinker
    //c |= isPtr()    && p == rhs->p;
    c |= isArray()  && a == rhs->a;
    c |= isVector() && v == rhs->v;
    // FIXME: add support for this: c |= isStruct() && s == rhs->s;
    return move(c) && typeVar() == rhs->typeVar();
  }
  assert(0 && "unhandled case in SymbolicType::operator==");
  UNREACHABLE();
}

expr SymbolicType::sameType(const Type &b) const {
  if (this == &b)
    return true;

  if (auto rhs = dynamic_cast<const IntType*>(&b))
    return isInt() && i.sameType(*rhs);
  if (auto rhs = dynamic_cast<const FloatType*>(&b))
    return isFloat() && f.sameType(*rhs);
  if (auto rhs = dynamic_cast<const PtrType*>(&b))
    return isPtr() && p.sameType(*rhs);
  if (auto rhs = dynamic_cast<const ArrayType*>(&b))
    return isArray() && a.sameType(*rhs);
  if (auto rhs = dynamic_cast<const VectorType*>(&b))
    return isVector() && v.sameType(*rhs);
  if (auto rhs = dynamic_cast<const StructType*>(&b))
    return isStruct() && s.sameType(*rhs);

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {
    expr c(false);
    c |= isInt()    && i.sameType(rhs->i);
    c |= isFloat()  && f.sameType(rhs->f);
    c |= isPtr()    && p.sameType(rhs->p);
    c |= isArray()  && a.sameType(rhs->a);
    c |= isVector() && v.sameType(rhs->v);
    // FIXME: add support for this: c |= isStruct() && s.sameType(rhs->s);
    return move(c) && typeVar() == rhs->typeVar();
  }
  assert(0 && "unhandled case in SymbolicType::sameType");
  UNREACHABLE();
}

void SymbolicType::fixup(const Model &m) {
  unsigned smt_typ = m.getUInt(typeVar());
  assert(smt_typ >= Int && smt_typ <= Struct);
  typ = TypeNum(smt_typ);

  switch (typ) {
  case Int:    i.fixup(m); break;
  case Float:  f.fixup(m); break;
  case Ptr:    p.fixup(m); break;
  case Array:  a.fixup(m); break;
  case Vector: v.fixup(m); break;
  case Struct: s.fixup(m); break;
  case Undefined:
    UNREACHABLE();
  }
}

expr SymbolicType::enforceIntType(unsigned bits) const {
  return isInt() && i.enforceIntType(bits);
}

expr SymbolicType::enforceIntOrVectorType() const {
  return isInt() || isVector();
}

expr SymbolicType::enforceIntOrPtrOrVectorType() const {
  return isInt() || isPtr() || isVector();
}

expr SymbolicType::enforcePtrType() const {
  return isPtr();
}

expr SymbolicType::enforceStructType() const {
  return isStruct();
}

expr SymbolicType::enforceAggregateType() const {
  return isArray() || isStruct();
}

expr SymbolicType::enforceFloatType() const {
  return isFloat();
}

const StructType* SymbolicType::getAsStructType() const {
  return &s;
}

const FloatType* SymbolicType::getAsFloatType() const {
  if (typ == Float)
    return &f;
  else
    return nullptr;
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
  case Struct:    s.print(os); break;
  case Undefined: break;
  }
}

}
