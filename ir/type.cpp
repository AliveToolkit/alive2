// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/type.h"
#include "ir/state.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <cassert>
#include <sstream>

using namespace smt;
using namespace std;

static constexpr unsigned var_type_bits = 3;
static constexpr unsigned var_bw_bits = 8;
static constexpr unsigned var_vector_elements = 10;


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
  return typeVar() == t;
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

bool Type::isIntType() const {
  return false;
}

bool Type::isFloatType() const {
  return false;
}

bool Type::isPtrType() const {
  return false;
}

bool Type::isArrayType() const {
  return false;
}

bool Type::isStructType() const {
  return false;
}

bool Type::isVectorType() const {
  return false;
}

bool Type::isAggregateType() const {
  return isArrayType() || isStructType() || isVectorType();
}

expr Type::enforceIntType(unsigned bits) const {
  return false;
}

expr Type::enforceIntOrPtrType() const {
  return enforceIntType() || enforcePtrType();
}

expr Type::enforcePtrType() const {
  return false;
}

expr Type::enforceStructType() const {
  return false;
}

expr Type::enforceAggregateType(vector<Type*> *element_types) const {
  return false;
}

expr Type::enforceFloatType() const {
  return false;
}

expr
Type::enforceVectorType(const function<expr(const Type&)> &enforceElem) const {
  return false;
}

expr Type::enforceScalarOrVectorType(
       const function<expr(const Type&)> &enforceElem) const {
  return enforceElem(*this) || enforceVectorType(enforceElem);
}

expr Type::enforceIntOrVectorType(unsigned bits) const {
  return enforceScalarOrVectorType(
           [&](auto &ty) { return ty.enforceIntType(bits); });
}

expr Type::enforceIntOrPtrOrVectorType() const {
  return enforceScalarOrVectorType(
           [&](auto &ty) { return ty.enforceIntOrPtrType(); });
}

expr Type::enforceFloatOrVectorType() const {
  return enforceScalarOrVectorType(
           [&](auto &ty) { return ty.enforceFloatType(); });
}

const FloatType* Type::getAsFloatType() const {
  return nullptr;
}

const AggregateType* Type::getAsAggregateType() const {
  return nullptr;
}

const StructType* Type::getAsStructType() const {
  return nullptr;
}

expr Type::toBV(expr e) const {
  return e;
}

StateValue Type::toBV(StateValue v) const {
  expr val = toBV(v.value);
  auto bw = val.bits();
  return { move(val),
           expr::mkIf(v.non_poison, expr::mkUInt(0, bw), expr::mkInt(-1, bw)) };
}

expr Type::fromBV(expr e) const {
  return e;
}

StateValue Type::fromBV(StateValue v) const {
  return { fromBV(v.value), v.non_poison == 0 };
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

StateValue VoidType::getDummyValue(bool non_poison) const {
  return { false, non_poison };
}

expr VoidType::getTypeConstraints() const {
  return true;
}

void VoidType::fixup(const Model &m) {
  // do nothing
}

pair<expr, expr>
VoidType::refines(const StateValue &src, const StateValue &tgt) const {
  return { true, true };
}

pair<expr, vector<expr>> VoidType::mkInput(State &s, const char *name) const {
  UNREACHABLE();
}

void VoidType::printVal(ostream &os, State &s, const expr &e) const {
  UNREACHABLE();
}

void VoidType::print(ostream &os) const {
  os << "void";
}


unsigned IntType::bits() const {
  return bitwidth;
}

StateValue IntType::getDummyValue(bool non_poison) const {
  return { expr::mkUInt(0, bits()), non_poison };
}

expr IntType::getTypeConstraints() const {
  // since size cannot be unbounded, limit it between 1 and 64 bits if undefined
  auto bw = sizeVar();
  auto r = bw != 0;
  if (!defined)
    r &= bw.ule(64);
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

bool IntType::isIntType() const {
  return true;
}

expr IntType::enforceIntType(unsigned bits) const {
  return bits ? sizeVar() == bits : true;
}

pair<expr, expr>
IntType::refines(const StateValue &src, const StateValue &tgt) const {
  return { src.non_poison.implies(tgt.non_poison),
           (src.non_poison && tgt.non_poison).implies(src.value == tgt.value) };
}

pair<expr, vector<expr>> IntType::mkInput(State &s, const char *name) const {
  auto var = expr::mkVar(name, bits());
  return { var, { var } };
}

void IntType::printVal(ostream &os, State &s, const expr &e) const {
  e.printHexadecimal(os);
  os << " (";
  e.printUnsigned(os);
  if (e.bits() > 1 && e.isSigned()) {
    os << ", ";
    e.printSigned(os);
  }
  os << ')';
}

void IntType::print(ostream &os) const {
  if (bits())
    os << 'i' << bits();
}


unsigned FloatType::bits() const {
  switch (fpType) {
  case Half:
    return 16;
  case Float:
    return 32;
  case Double:
    return 64;
  case Unknown:
    UNREACHABLE();
  }
  UNREACHABLE();
}

const FloatType* FloatType::getAsFloatType() const {
  return this;
}

expr FloatType::toBV(expr e) const {
  return e.float2BV();
}

StateValue FloatType::toBV(StateValue v) const {
  return Type::toBV(move(v));
}

expr FloatType::fromBV(expr e) const {
  return e.BV2float(getDummyValue(true).value);
}

StateValue FloatType::fromBV(StateValue v) const {
  return Type::fromBV(move(v));
}

expr FloatType::sizeVar() const {
  return defined ? expr::mkUInt(getFpType(), var_bw_bits) : Type::sizeVar();
}

StateValue FloatType::getDummyValue(bool non_poison) const {
  expr e;
  switch (fpType) {
  case Half:    e = expr::mkHalf(0); break;
  case Float:   e = expr::mkFloat(0); break;
  case Double:  e = expr::mkDouble(0); break;
  case Unknown: UNREACHABLE();
  }
  return { move(e), non_poison };
}

expr FloatType::getTypeConstraints() const {
  if (defined)
    return true;
  return sizeVar().ult(Unknown);
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
  assert(fp_typ < (unsigned)Unknown);
  fpType = FpType(fp_typ);
}

bool FloatType::isFloatType() const {
  return true;
}

expr FloatType::enforceFloatType() const {
  return true;
}

pair<expr, expr>
FloatType::refines(const StateValue &src, const StateValue &tgt) const {
  expr non_poison = src.non_poison && tgt.non_poison;
  return { src.non_poison.implies(tgt.non_poison),
           (non_poison && !src.value.isNaN()).implies(src.value == tgt.value) };
}

pair<expr, vector<expr>> FloatType::mkInput(State &s, const char *name) const {
  expr var;
  switch (fpType) {
  case Half:    var = expr::mkHalfVar(name); break;
  case Float:   var = expr::mkFloatVar(name); break;
  case Double:  var = expr::mkDoubleVar(name); break;
  case Unknown: UNREACHABLE();
  }
  return { var, { var } };
}

void FloatType::printVal(ostream &os, State &s, const expr &e) const {
  e.float2BV().printHexadecimal(os);
  os << " (";
  if (e.isNaN().simplify().isTrue()) {
    os << "NaN";
  } else if (e.isFPZero().simplify().isTrue()) {
    os << (e.isFPNeg().simplify().isTrue() ? "-0.0" : "+0.0");
  } else if (e.isInf().simplify().isTrue()) {
    os << (e.isFPNeg().simplify().isTrue() ? "-oo" : "+oo");
  } else {
    os << e.float2Real().simplify().numeral_string();
  }
  os << ')';
}

void FloatType::print(ostream &os) const {
  switch (fpType) {
  case Half:
    os << "half";
    break;
  case Float:
    os << "float";
    break;
  case Double:
    os << "double";
    break;
  case Unknown:
    break;
  }
}


PtrType::PtrType(unsigned addr_space)
  : Type(addr_space == 0 ? "*" : "as(" + to_string(addr_space) + ")*"),
    addr_space(addr_space), defined(true) {}

expr PtrType::ASVar() const {
  return defined ? expr::mkUInt(addr_space, 2) : var("as", 2);
}

unsigned PtrType::bits() const {
  // TODO: make this configurable
  return 64+8+8;
}

StateValue PtrType::getDummyValue(bool non_poison) const {
  return { expr::mkUInt(0, bits()), non_poison };
}

expr PtrType::getTypeConstraints() const {
  return sizeVar() == bits();
}

expr PtrType::sizeVar() const {
  return defined ? expr::mkUInt(bits(), var_bw_bits) : Type::sizeVar();
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

bool PtrType::isPtrType() const {
  return true;
}

expr PtrType::enforcePtrType() const {
  return true;
}

pair<expr, expr>
PtrType::refines(const StateValue &src, const StateValue &tgt) const {
  return { src.non_poison.implies(tgt.non_poison),
           (src.non_poison && tgt.non_poison).implies(src.value == tgt.value) };
}

pair<expr, vector<expr>> PtrType::mkInput(State &s, const char *name) const {
  return s.getMemory().mkInput(name);
}

void PtrType::printVal(ostream &os, State &s, const expr &e) const {
  os << Pointer(s.getMemory(), e);
}

void PtrType::print(ostream &os) const {
  if (addr_space != 0)
    os << "as(" << addr_space << ")*";
  else
    os << '*';
}


AggregateType::AggregateType(string &&name, bool symbolic)
  : Type(string(name)) {
  if (!symbolic)
    return;

  // create symbolic type with a finite number of children
  unsigned num = 4;
  sym.resize(num);
  children.resize(num);

  // FIXME: limitation below is for vectors; what about structs and arrays?
  for (unsigned i = 0; i < num; ++i) {
    sym[i] = make_unique<SymbolicType>("v#" + to_string(i) + '_' + name,
                                        (1 << SymbolicType::Int) |
                                        (1 << SymbolicType::Float) |
                                        (1 << SymbolicType::Ptr));
    children[i] = sym[i].get();
  }
}

expr AggregateType::numElements() const {
  return defined ? expr::mkUInt(elements, var_vector_elements) :
                   var("elements", var_vector_elements);
}

StateValue AggregateType::aggregateVals(const vector<StateValue> &vals) const {
  assert(vals.size() == elements);
  // structs can be empty
  if (vals.empty())
    return { expr::mkUInt(0, 1), true };

  StateValue v;
  for (unsigned idx = 0; idx < elements; ++idx) {
    auto vv = children[idx]->toBV(vals[idx]);
    v = idx == 0 ? move(vv) : v.concat(vv);
  }
  return v;
}

StateValue AggregateType::extract(const StateValue &val, unsigned index) const {
  unsigned total_till_now = 0;
  for (unsigned i = 0; i < index; ++i) {
    total_till_now += children[i]->bits();
  }
  unsigned high = val.value.bits() - total_till_now;
  unsigned h = high - 1;
  unsigned l = high - children[index]->bits();

  return children[index]->fromBV({ val.value.extract(h, l),
                                   val.non_poison.extract(h, l) });
}

StateValue AggregateType::extract(const StateValue &val,
                                  const expr &index) const {
  auto &elementTy = *children[0];
  for (unsigned i = 1; i < elements; ++i) {
    assert(elementTy.bits() == children[i]->bits());
  }
  unsigned bw_elem = elementTy.bits();
  unsigned bw_val = val.value.bits();
  assert(bw_val == val.non_poison.bits());
  expr idx = index.zextOrTrunc(bw_val) * expr::mkUInt(bw_elem, bw_val);

  unsigned h = elements * bw_elem - 1;
  unsigned l = (elements - 1) * bw_elem;

  return elementTy.fromBV({ (val.value << idx).extract(h, l),
                            (val.non_poison << idx).extract(h, l) });
}

unsigned AggregateType::bits() const {
  unsigned bw = 0;
  for (unsigned i = 0; i < elements; ++i) {
    bw += children[i]->bits();
  }
  return bw;
}

StateValue AggregateType::getDummyValue(bool non_poison) const {
  vector<StateValue> vals;
  for (unsigned i = 0; i < elements; ++i) {
    vals.emplace_back(children[i]->getDummyValue(non_poison));
  }
  return aggregateVals(vals);
}

expr AggregateType::getTypeConstraints() const {
  expr r(true), elems = numElements();
  for (unsigned i = 0, e = children.size(); i != e; ++i) {
    r &= elems.ugt(i).implies(children[i]->getTypeConstraints());
  }
  if (!defined)
    r &= elems.ule(4);
  return r;
}

expr AggregateType::operator==(const AggregateType &rhs) const {
  expr elems = numElements();
  expr res = elems == rhs.numElements();
  for (unsigned i = 0, e = min(children.size(), rhs.children.size());
       i != e; ++i) {
    res &= elems.ugt(i).implies(*children[i] == *rhs.children[i]);
  }
  return res;
}

expr AggregateType::sameType(const AggregateType &rhs) const {
  expr elems = numElements();
  expr res = elems == rhs.numElements();
  for (unsigned i = 0, e = min(children.size(), rhs.children.size());
       i != e; ++i) {
    res &= elems.ugt(i).implies(children[i]->sameType(*rhs.children[i]));
  }
  return res;
}

void AggregateType::fixup(const Model &m) {
  if (!defined)
    elements = m.getUInt(numElements());

  for (unsigned i = 0; i < elements; ++i) {
    children[i]->fixup(m);
  }
}

expr AggregateType::enforceAggregateType(vector<Type*> *element_types) const {
  if (!element_types)
    return true;

  if (children.size() < element_types->size())
    return false;

  expr r = numElements() == element_types->size();
  for (unsigned i = 0, e = element_types->size(); i != e; ++i) {
    r &= *children[i] == *(*element_types)[i];
  }
  return r;
}

expr AggregateType::toBV(expr e) const {
  return Type::toBV(move(e));
}

StateValue AggregateType::toBV(StateValue v) const {
  return v;
}

expr AggregateType::fromBV(expr e) const {
  return Type::fromBV(move(e));
}

StateValue AggregateType::fromBV(StateValue v) const {
  return v;
}

pair<expr, expr>
AggregateType::refines(const StateValue &src, const StateValue &tgt) const {
  set<expr> poison, value;
  for (unsigned i = 0; i < elements; ++i) {
    auto [p, v] = children[i]->refines(extract(src, i), extract(tgt, i));
    poison.insert(move(p));
    value.insert(move(v));
  }
  return { expr::mk_and(poison), expr::mk_and(value) };
}

pair<expr, vector<expr>>
AggregateType::mkInput(State &s, const char *name) const {
  expr val;
  vector<expr> vars;

  for (unsigned i = 0; i < elements; ++i) {
    string c_name = string(name) + "#" + to_string(i);
    auto [v, vs] = children[i]->mkInput(s, c_name.c_str());
    v = children[i]->toBV(move(v));
    val = i == 0 ? move(v) : val.concat(v);
    vars.insert(vars.end(), vs.begin(), vs.end());
  }
  return { move(val), move(vars) };
}

void AggregateType::printVal(ostream &os, State &s, const expr &e) const {
  UNREACHABLE();
}

const AggregateType* AggregateType::getAsAggregateType() const {
  return this;
}


expr ArrayType::getTypeConstraints() const {
  // TODO
  return false;
}

bool ArrayType::isArrayType() const {
  return true;
}

void ArrayType::print(ostream &os) const {
  os << "TODO";
}


VectorType::VectorType(string &&name, unsigned elements, Type &elementTy)
  : AggregateType(move(name), false) {
  assert(elements != 0);
  this->elements = elements;
  defined = true;
  children.resize(elements, &elementTy);
}

expr VectorType::getTypeConstraints() const {
  auto &elementTy = *children[0];
  expr r = AggregateType::getTypeConstraints() &&
           (elementTy.enforceIntType() ||
            elementTy.enforceFloatType() ||
            elementTy.enforcePtrType()) &&
           numElements() != 0;

  // all elements have the same type
  for (unsigned i = 1, e = children.size(); i != e; ++i) {
    r &= numElements().ugt(i).implies(elementTy == *children[i]);
  }

  return r;
}

bool VectorType::isVectorType() const {
  return true;
}

expr VectorType::enforceVectorType(
    const function<expr(const Type&)> &enforceElem) const {
  return enforceElem(*children[0]);
}

void VectorType::print(ostream &os) const {
  if (elements)
    os << '<' << elements << " x " << *children[0] << '>';
}


StructType::StructType(string &&name, vector<Type*> &&children)
  : AggregateType(move(name), move(children)) {
  elements = this->children.size();
  defined = true;
}

bool StructType::isStructType() const {
  return true;
}

expr StructType::enforceStructType() const {
  return true;
}

const StructType* StructType::getAsStructType() const {
  return this;
}

void StructType::print(ostream &os) const {
  if (!elements)
    return;

  os << '{';
  for (unsigned i = 0; i < elements; ++i) {
    if (i != 0)
      os << ", ";
    children[i]->print(os);
  }
  os << '}';
}


SymbolicType::SymbolicType(string &&name)
  : Type(string(name)), i(string(name)), f(string(name)), p(string(name)),
    a(string(name)), v(string(name)), s(string(name)) {}

SymbolicType::SymbolicType(string &&name, unsigned type_mask)
  : Type(string(name)) {
  if (type_mask & (1 << Int))
    i.emplace(string(name));
  if (type_mask & (1 << Float))
    f.emplace(string(name));
  if (type_mask & (1 << Ptr))
    p.emplace(string(name));
  if (type_mask & (1 << Array))
    a.emplace(string(name));
  if (type_mask & (1 << Vector))
    v.emplace(string(name));
  if (type_mask & (1 << Struct))
    s.emplace(string(name));
}

#define DISPATCH(call, undef)     \
  switch (typ) {                  \
  case Int:       return i->call; \
  case Float:     return f->call; \
  case Ptr:       return p->call; \
  case Array:     return a->call; \
  case Vector:    return v->call; \
  case Struct:    return s->call; \
  case Undefined: undef;          \
  }                               \
  UNREACHABLE()

unsigned SymbolicType::bits() const {
  DISPATCH(bits(), UNREACHABLE());
}

StateValue SymbolicType::getDummyValue(bool non_poison) const {
  DISPATCH(getDummyValue(non_poison), UNREACHABLE());
}

expr SymbolicType::getTypeConstraints() const {
  expr c(false);
  if (i) c |= isInt()    && i->getTypeConstraints();
  if (f) c |= isFloat()  && f->getTypeConstraints();
  if (p) c |= isPtr()    && p->getTypeConstraints();
  if (a) c |= isArray()  && a->getTypeConstraints();
  if (v) c |= isVector() && v->getTypeConstraints();
  if (s) c |= isStruct() && s->getTypeConstraints();
  return c;
}

expr SymbolicType::operator==(const Type &b) const {
  if (this == &b)
    return true;

  if (auto rhs = dynamic_cast<const IntType*>(&b))
    return isInt() && (i ? *i == *rhs : false);
  if (auto rhs = dynamic_cast<const FloatType*>(&b))
    return isFloat() && (f ? *f == *rhs : false);
  if (auto rhs = dynamic_cast<const PtrType*>(&b))
    return isPtr() && (p ? *p == *rhs : false);
  if (auto rhs = dynamic_cast<const ArrayType*>(&b))
    return isArray() && (a ? *a == *rhs : false);
  if (auto rhs = dynamic_cast<const VectorType*>(&b))
    return isVector() && (v ? *v == *rhs : false);
  if (auto rhs = dynamic_cast<const StructType*>(&b))
    return isStruct() && (s ? *s == *rhs : false);

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {
    expr c(false);
    if (i && rhs->i) c |= isInt()    && *i == *rhs->i;
    if (f && rhs->f) c |= isFloat()  && *f == *rhs->f;
    if (p && rhs->p) c |= isPtr()    && *p == *rhs->p;
    if (a && rhs->a) c |= isArray()  && *a == *rhs->a;
    if (v && rhs->v) c |= isVector() && *v == *rhs->v;
    if (s && rhs->s) c |= isStruct() && *s == *rhs->s;
    return move(c) && typeVar() == rhs->typeVar();
  }
  assert(0 && "unhandled case in SymbolicType::operator==");
  UNREACHABLE();
}

expr SymbolicType::sameType(const Type &b) const {
  if (this == &b)
    return true;

  if (auto rhs = dynamic_cast<const IntType*>(&b))
    return isInt() && (i ? i->sameType(*rhs) : false);
  if (auto rhs = dynamic_cast<const FloatType*>(&b))
    return isFloat() && (f ? f->sameType(*rhs) : false);
  if (auto rhs = dynamic_cast<const PtrType*>(&b))
    return isPtr() && (p ? p->sameType(*rhs) : false);
  if (auto rhs = dynamic_cast<const ArrayType*>(&b))
    return isArray() && (a ? a->sameType(*rhs) : false);
  if (auto rhs = dynamic_cast<const VectorType*>(&b))
    return isVector() && (v ? v->sameType(*rhs) : false);
  if (auto rhs = dynamic_cast<const StructType*>(&b))
    return isStruct() && (s ? s->sameType(*rhs) : false);

  if (auto rhs = dynamic_cast<const SymbolicType*>(&b)) {
    expr c(false);
    if (i && rhs->i) c |= isInt()    && i->sameType(*rhs->i);
    if (f && rhs->f) c |= isFloat()  && f->sameType(*rhs->f);
    if (p && rhs->p) c |= isPtr()    && p->sameType(*rhs->p);
    if (a && rhs->a) c |= isArray()  && a->sameType(*rhs->a);
    if (v && rhs->v) c |= isVector() && v->sameType(*rhs->v);
    if (s && rhs->s) c |= isStruct() && s->sameType(*rhs->s);
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
  case Int:    i->fixup(m); break;
  case Float:  f->fixup(m); break;
  case Ptr:    p->fixup(m); break;
  case Array:  a->fixup(m); break;
  case Vector: v->fixup(m); break;
  case Struct: s->fixup(m); break;
  case Undefined:
    UNREACHABLE();
  }
}

bool SymbolicType::isIntType() const {
  return typ == Int;
}

bool SymbolicType::isFloatType() const {
  return typ == Float;
}

bool SymbolicType::isPtrType() const {
  return typ == Ptr;
}

bool SymbolicType::isArrayType() const {
  return typ == Array;
}

bool SymbolicType::isVectorType() const {
  return typ == Vector;
}

bool SymbolicType::isStructType() const {
  return typ == Struct;
}

expr SymbolicType::enforceIntType(unsigned bits) const {
  return isInt() && (i ? i->enforceIntType(bits) : false);
}

expr SymbolicType::enforcePtrType() const {
  return isPtr();
}

expr SymbolicType::enforceStructType() const {
  return isStruct();
}

expr SymbolicType::enforceAggregateType(vector<Type*> *element_types) const {
  return (isArray()  && a->enforceAggregateType(element_types)) ||
         (isVector() && v->enforceAggregateType(element_types)) ||
         (isStruct() && s->enforceAggregateType(element_types));
}

expr SymbolicType::enforceFloatType() const {
  return isFloat();
}

expr SymbolicType::enforceVectorType(
    const function<expr(const Type&)> &enforceElem) const {
  return v ? (isVector() && v->enforceVectorType(enforceElem)) : false;
}

const FloatType* SymbolicType::getAsFloatType() const {
  return &*f;
}

const AggregateType* SymbolicType::getAsAggregateType() const {
  switch (typ) {
  case Int:
  case Float:
  case Ptr:
    return nullptr;
  case Array:     return &*a;
  case Vector:    return &*v;
  case Struct:    return &*s;
  case Undefined: return &*s; // FIXME: needs a proxy or something
  }
  UNREACHABLE();
}

const StructType* SymbolicType::getAsStructType() const {
  return &*s;
}

expr SymbolicType::toBV(expr e) const {
  DISPATCH(toBV(move(e)), UNREACHABLE());
}

StateValue SymbolicType::toBV(StateValue val) const {
  DISPATCH(toBV(move(val)), UNREACHABLE());
}

expr SymbolicType::fromBV(expr e) const {
  DISPATCH(fromBV(move(e)), UNREACHABLE());
}

StateValue SymbolicType::fromBV(StateValue val) const {
  DISPATCH(fromBV(move(val)), UNREACHABLE());
}

pair<expr, expr>
SymbolicType::refines(const StateValue &src, const StateValue &tgt) const {
  DISPATCH(refines(src, tgt), UNREACHABLE());
}

pair<expr, vector<expr>>
SymbolicType::mkInput(State &st, const char *name) const {
  DISPATCH(mkInput(st, name), UNREACHABLE());
}

void SymbolicType::printVal(ostream &os, State &st, const expr &e) const {
  DISPATCH(printVal(os, st, e), UNREACHABLE());
}

void SymbolicType::print(ostream &os) const {
  DISPATCH(print(os), return);
}

}
