// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/type.h"
#include "ir/globals.h"
#include "ir/state.h"
#include "smt/solver.h"
#include "util/compiler.h"
#include <array>
#include <cassert>
#include <numeric>
#include <sstream>

using namespace smt;
using namespace util;
using namespace std;

static constexpr unsigned var_type_bits = 3;
static constexpr unsigned var_bw_bits = 11;
static constexpr unsigned var_vector_elements = 10;


namespace IR {

VoidType Type::voidTy;

unsigned Type::np_bits() const {
  auto bw = bits();
  return min(bw, (unsigned)divide_up(bw * bits_poison_per_byte, bits_byte));
}

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

expr Type::scalarSize() const {
  return sizeVar();
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

expr Type::enforceScalarType() const {
  return enforceIntType() || enforcePtrType() || enforceFloatType();
}

expr Type::enforceVectorType() const {
  return enforceVectorType([](auto &ty) { return true; });
}

expr Type::enforceVectorTypeIff(const Type &other) const {
  expr elems = false;
  if (auto agg = getAsAggregateType())
    if (auto agg2 = other.getAsAggregateType())
      elems = agg->numElements() == agg2->numElements();
  return !other.enforceVectorType() || (enforceVectorType() && elems);
}

expr Type::enforceVectorTypeSameChildTy(const Type &other) const {
  if (auto agg = getAsAggregateType())
    if (auto agg2 = other.getAsAggregateType())
      return agg->getChild(0) == agg2->getChild(0);
  return false;
}

expr Type::enforceVectorTypeEquiv(const Type &other) const {
  return enforceVectorTypeIff(other) && other.enforceVectorTypeIff(*this);
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

expr Type::enforceIntOrFloatOrPtrOrVectorType() const {
  return enforceScalarOrVectorType(
    [&](auto &ty) { return ty.enforceIntOrPtrType() || ty.enforceFloatType();});
}

expr Type::enforceIntOrPtrOrVectorType() const {
  return enforceScalarOrVectorType(
           [&](auto &ty) { return ty.enforceIntOrPtrType(); });
}

expr Type::enforceFloatOrVectorType() const {
  return enforceScalarOrVectorType(
           [&](auto &ty) { return ty.enforceFloatType(); });
}

expr Type::enforcePtrOrVectorType() const {
  return enforceScalarOrVectorType(
           [&](auto &ty) { return ty.enforcePtrType(); });
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
  auto bw = np_bits();
  return { toBV(std::move(v.value)),
           expr::mkIf(v.non_poison, expr::mkInt(-1, bw), expr::mkUInt(0, bw)) };
}

expr Type::fromBV(expr e) const {
  return e;
}

StateValue Type::fromBV(StateValue v) const {
  return { fromBV(std::move(v.value)),
           v.non_poison == expr::mkInt(-1, v.non_poison) };
}

expr Type::toInt(State &s, expr v) const {
  return toBV(std::move(v));
}

StateValue Type::toInt(State &s, StateValue v) const {
  auto bw = np_bits();
  return { toInt(s, std::move(v.value)),
           expr::mkIf(v.non_poison, expr::mkInt(-1, bw), expr::mkUInt(0, bw)) };
}

expr Type::fromInt(expr e) const {
  return fromBV(std::move(e));
}

StateValue Type::fromInt(StateValue v) const {
  return { fromInt(std::move(v.value)),
           v.non_poison.isBool()
             ? expr(v.non_poison)
             : v.non_poison == expr::mkInt(-1, v.non_poison) };
}

expr Type::combine_poison(const expr &boolean, const expr &orig) const {
  return
    expr::mkIf(boolean, expr::mkInt(-1, orig), expr::mkInt(0, orig)) & orig;
}

StateValue Type::mkUndef(State &s) const {
  auto val = getDummyValue(true);
  expr var = expr::mkFreshVar("undef", val.value);
  s.addUndefVar(expr(var));
  return { std::move(var), std::move(val.non_poison) };
}

pair<expr, expr> Type::mkUndefInput(State &s, const ParamAttrs &attrs) const {
  auto var = expr::mkFreshVar("undef", mkInput(s, "", attrs));
  return { var, var };
}

ostream& operator<<(ostream &os, const Type &t) {
  t.print(os);
  return os;
}

string Type::toString() const {
  ostringstream s;
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
VoidType::refines(State &src_s, State &tgt_s, const StateValue &src,
                  const StateValue &tgt) const {
  return { true, true };
}

expr VoidType::mkInput(State &s, const char *name,
                       const ParamAttrs &attrs) const {
  UNREACHABLE();
}

void VoidType::printVal(ostream &os, const State &s, const Model &m,
                        const expr &e) const {
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
IntType::refines(State &src_s, State &tgt_s, const StateValue &src,
                 const StateValue &tgt) const {
  return { src.non_poison.implies(tgt.non_poison),
           (src.non_poison && tgt.non_poison).implies(src.value == tgt.value) };
}

expr IntType::mkInput(State &s, const char *name,
                      const ParamAttrs &attrs) const {
  return expr::mkVar(name, bits());
}

void IntType::printVal(ostream &os, const State &s, const Model &m,
                       const expr &e) const {
  e.printHexadecimal(os);
  os << " (";
  e.printUnsigned(os);
  if (e.bits() > 1 && e.isNegative().isTrue()) {
    os << ", ";
    e.printSigned(os);
  }
  os << ')';
}

void IntType::print(ostream &os) const {
  if (bits())
    os << 'i' << bits();
}


// total bits, exponent bits
static array<pair<unsigned, unsigned>, 5> float_sizes = {
  /* Half */   make_pair(16, 5),
  /* Float */  make_pair(32, 8),
  /* Double */ make_pair(64, 11),
  /* Quad */   make_pair(128, 15),
  /* BFloat */ make_pair(16, 8),
};

expr FloatType::getDummyFloat() const {
  expr ty;
  switch (fpType) {
  case Half:    ty = expr::mkHalf(0); break;
  case Float:   ty = expr::mkFloat(0); break;
  case Double:  ty = expr::mkDouble(0); break;
  case Quad:    ty = expr::mkQuad(0); break;
  case BFloat:  ty = expr::mkBFloat(0); break;
  case Unknown: UNREACHABLE();
  }
  return ty;
}

expr FloatType::getFloat(const expr &v) const {
  expr ty = getDummyFloat();

  if (isNaNInt(v))
    return expr::mkNaN(ty);

  expr cond, then, els, n, n2;
  // match (ite (isNaN x) int_nan (fp.to_ieee_bv x))
  if (v.isIf(cond, then, els) &&
      cond.isNaNCheck(n) &&
      isNaNInt(then) &&
      els.isfloat2BV(n2) &&
      n.eq(n2))
    return n;

  return v.BV2float(ty);
}

expr FloatType::mkNaN(State &s, bool canonical) const {
  unsigned exp_bits = float_sizes[fpType].second;
  unsigned fraction_bits = bits() - exp_bits - 1;
  unsigned var_bits = fraction_bits + 1;

  // NaN has a non-deterministic non-zero fraction bit pattern
  expr zero = expr::mkUInt(0, var_bits);
  expr var = canonical ? expr::mkVar("#NaN_canonical", zero)
                       : expr::mkFreshVar("#NaN", zero);
  expr fraction = var.extract(fraction_bits - 1, 0);

  s.addPre(fraction != 0);
  // TODO s.addPre(expr::mkUF("isQNaN", { fraction }, false));
  if (!canonical)
    s.addQuantVar(var);

  // sign bit, exponent (-1), fraction (non-zero)
  return var.extract(fraction_bits, fraction_bits)
            .concat(expr::mkInt(-1, exp_bits))
            .concat(fraction);
}

expr FloatType::fromFloat(State &s, const expr &fp) const {
  expr isnan = fp.isNaN();
  expr val = fp.float2BV();

  if (isnan.isFalse())
    return val;
  return expr::mkIf(isnan, mkNaN(s, false), val);
}

expr FloatType::isNaN(const expr &v, bool signalling) const {
  unsigned exp_bits = float_sizes[fpType].second;
  unsigned fraction_bits = bits() - exp_bits - 1;

  expr exponent = v.extract(fraction_bits + exp_bits - 1, fraction_bits);
  expr fraction = v.extract(fraction_bits - 1, 0);
  expr isqnan = expr::mkUF("isQNaN", { fraction }, false);
  if (signalling)
    isqnan = !isqnan;
  return exponent == -1u && fraction != 0 && isqnan;
}

unsigned FloatType::bits() const {
  assert(fpType != Unknown);
  return float_sizes[fpType].first;
}

const FloatType* FloatType::getAsFloatType() const {
  return this;
}

bool FloatType::isNaNInt(const expr &e) const {
  if (!e.isValid())
    return false;

  auto bw = e.bits();
  unsigned exp_bits = float_sizes[fpType].second;
  unsigned fraction_bits = bw - exp_bits - 1;

  expr sign = e.extract(bw - 1, bw - 1);
  expr exponent = e.extract(bw - 2, fraction_bits);
  expr fraction = e.extract(fraction_bits - 1, 0);
  assert(exponent.bits() == exp_bits);

  expr nan, nan2;
  unsigned h, l;
  bool ok = sign.isExtract(nan, h, l) &&
            fraction.isExtract(nan2, h, l) &&
            nan.eq(nan2) &&
            nan.fn_name().starts_with("#NaN");

  return ok && exponent.isAllOnes();
}

expr FloatType::sizeVar() const {
  return defined ? expr::mkUInt(bits(), var_bw_bits) : Type::sizeVar();
}

StateValue FloatType::getDummyValue(bool non_poison) const {
  return { expr::mkUInt(0, bits()), non_poison };
}

expr FloatType::getTypeConstraints() const {
  if (defined)
    return true;

  expr r(false);
  for (auto sz : float_sizes) {
    r |= sizeVar() == sz.first;
  }
  return r;
}

expr FloatType::operator==(const FloatType &rhs) const {
  return sizeVar() == rhs.sizeVar();
}

void FloatType::fixup(const Model &m) {
  if (defined)
    return;

  unsigned m_sz = m.getUInt(sizeVar());
  for (unsigned i = 0, e = float_sizes.size(); i != e; ++i) {
    if (m_sz == float_sizes[i].first) {
      fpType = FpType(i);
      return;
    }
  }
  UNREACHABLE();
}

bool FloatType::isFloatType() const {
  return true;
}

expr FloatType::enforceFloatType() const {
  return true;
}

pair<expr, expr>
FloatType::refines(State &src_s, State &tgt_s, const StateValue &src,
                   const StateValue &tgt) const {
  expr non_poison = src.non_poison && tgt.non_poison;
  return { src.non_poison.implies(tgt.non_poison),
           (src.non_poison && tgt.non_poison).implies(src.value == tgt.value) };
}

expr FloatType::mkInput(State &s, const char *name,
                        const ParamAttrs &attrs) const {
  return expr::mkVar(name, bits());
}

void FloatType::printVal(ostream &os, const State &s, const Model &m,
                         const expr &e) const {
  e.printHexadecimal(os);
  auto f = getFloat(e);
  os << " (";
  if (m.eval(isNaN(e, true)).isTrue()) {
    os << "SNaN";
  } else if (m.eval(isNaN(e, false)).isTrue()) {
    os << "QNaN";
  } else if (f.isNaN().isTrue()) {
    os << "NaN";
  } else if (f.isFPZero().isTrue()) {
    os << (f.isFPNegative().isTrue() ? "-0.0" : "+0.0");
  } else if (f.isInf().isTrue()) {
    os << (f.isFPNegative().isTrue() ? "-oo" : "+oo");
  } else {
    os << f.float2Real().numeral_string();
  }
  os << ')';
}

void FloatType::print(ostream &os) const {
  const char *name = "";
  switch (fpType) {
  case Half:   name = "half"; break;
  case Float:  name = "float"; break;
  case Double: name = "double"; break;
  case Quad:   name = "fp128"; break;
  case BFloat: name = "bfloat"; break;
  case Unknown: break;
  }
  os << name;
}


PtrType::PtrType(unsigned addr_space)
  : Type(addr_space == 0 ? "ptr"
                         : "ptr addrspace(" + to_string(addr_space) + ')'),
    addr_space(addr_space), defined(true) {}

expr PtrType::ASVar() const {
  return defined ? expr::mkUInt(addr_space, 2) : var("as", 2);
}

unsigned PtrType::bits() const {
  return Pointer::totalBits();
}

unsigned PtrType::np_bits() const {
  return 1;
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

expr PtrType::toInt(State &s, expr v) const {
  return v;
}

StateValue PtrType::toInt(State &s, StateValue v) const {
  return Type::toInt(s, std::move(v));
}

expr PtrType::fromInt(expr v) const {
  return v;
}

StateValue PtrType::fromInt(StateValue v) const {
  return Type::fromInt(std::move(v));
}

pair<expr, expr>
PtrType::refines(State &src_s, State &tgt_s, const StateValue &src,
                 const StateValue &tgt) const {
  auto &sm = src_s.returnMemory(), &tm = tgt_s.returnMemory();
  Pointer p(sm, src.value);
  Pointer q(tm, tgt.value);

  return { src.non_poison.implies(tgt.non_poison),
           (src.non_poison && tgt.non_poison).implies(p.refined(q)) };
}

StateValue PtrType::mkUndef(State &s) const {
  return { Pointer::mkUndef(s), true };
}

expr PtrType::mkInput(State &s, const char *name,
                      const ParamAttrs &attrs) const {
  return s.getMemory().mkInput(name, attrs);
}

pair<expr, expr>
PtrType::mkUndefInput(State &s, const ParamAttrs &attrs) const {
  return s.getMemory().mkUndefInput(attrs);
}

void PtrType::printVal(ostream &os, const State &s, const Model &m,
                       const expr &e) const {
  os << Pointer(s.getMemory(), e);
}

void PtrType::print(ostream &os) const {
  os << "ptr";
  if (addr_space != 0)
    os << " addrspace(" << addr_space << ')';
}


AggregateType::AggregateType(string &&name, bool symbolic)
  : Type(string(name)) {
  if (!symbolic)
    return;

  // create symbolic type with a finite number of children
  elements = 4;
  sym.resize(elements);
  children.resize(elements);
  is_padding.resize(elements);

  // FIXME: limitation below is for vectors; what about structs and arrays?
  for (unsigned i = 0; i < elements; ++i) {
    sym[i] = make_unique<SymbolicType>("v#" + to_string(i) + '_' + name,
                                        (1 << SymbolicType::Int) |
                                        (1 << SymbolicType::Float) |
                                        (1 << SymbolicType::Ptr));
    children[i] = sym[i].get();
  }
}

AggregateType::AggregateType(string &&name, vector<Type*> &&vchildren,
                             vector<bool> &&vis_padding)
: Type(std::move(name)), children(std::move(vchildren)), is_padding(std::move(vis_padding)) {
  assert(children.size() == is_padding.size());
  elements = children.size();
}

expr AggregateType::numElements() const {
  return defined ? expr::mkUInt(elements, var_vector_elements) :
                   var("elements", var_vector_elements);
}

unsigned AggregateType::numPaddingsConst() const {
  return is_padding.empty() ? 0 : countPaddings(is_padding.size() - 1);
}

unsigned AggregateType::countPaddings(unsigned to_idx) const {
  unsigned count = 0;
  for (unsigned i = 0; i <= to_idx; ++i)
    count += is_padding[i];
  return count;
}

expr AggregateType::numElementsExcludingPadding() const {
  auto elems = numElements();
  return numElements() - expr::mkInt(numPaddingsConst(), elems);
}

StateValue AggregateType::aggregateVals(const vector<StateValue> &vals) const {
  assert(vals.size() + numPaddingsConst() == elements);
  // structs can be empty
  if (elements == 0)
    return { expr::mkUInt(0, 1), expr::mkUInt(0, 1) };

  StateValue v;
  bool first = true;
  unsigned val_idx = 0;
  for (unsigned idx = 0; idx < elements; ++idx) {
    if (children[idx]->bits() == 0) {
      assert(!isPadding(idx));
      val_idx++;
      continue;
    }

    StateValue vv;
    if (isPadding(idx))
      vv = children[idx]->getDummyValue(false);
    else
      vv = vals[val_idx++];
    vv = children[idx]->toBV(std::move(vv));
    v = first ? std::move(vv) : v.concat(vv);
    first = false;
  }
  return v;
}

StateValue
AggregateType::extract(const StateValue &val, unsigned index, bool fromInt)
    const {
  unsigned total_value = 0, total_np = 0;
  for (unsigned i = 0; i < index; ++i) {
    total_value += children[i]->bits();
    total_np += children[i]->np_bits();
  }

  unsigned h_val, l_val, h_np, l_np;
  if (fromInt && little_endian) {
    h_val = total_value + children[index]->bits() - 1;
    l_val = total_value;

    h_np = total_np + children[index]->np_bits() - 1;
    l_np = total_np;
  } else {
    unsigned high_val = bits() - total_value;
    h_val = high_val - 1;
    l_val = high_val - children[index]->bits();

    unsigned high_np = np_bits() - total_np;
    h_np = high_np - 1;
    l_np = high_np - children[index]->np_bits();
  }

  StateValue sv(val.value.extract(h_val, l_val),
                val.non_poison.extract(h_np, l_np));
  return fromInt ? children[index]->fromInt(std::move(sv)) :
                   children[index]->fromBV(std::move(sv));
}

unsigned AggregateType::bits() const {
  if (elements == 0)
    // It is set as 1 because zero-width bitvector is invalid.
    return 1;

  unsigned bw = 0;
  for (unsigned i = 0; i < elements; ++i) {
    bw += children[i]->bits();
  }
  return bw;
}

unsigned AggregateType::np_bits() const {
  if (elements == 0)
    // It is set as 1 because zero-width bitvector is invalid.
    return 1;

  unsigned bw = 0;
  for (unsigned i = 0; i < elements; ++i) {
    bw += children[i]->np_bits();
  }
  return bw;
}

StateValue AggregateType::getDummyValue(bool non_poison) const {
  vector<StateValue> vals;
  for (unsigned i = 0; i < elements; ++i) {
    if (!isPadding(i))
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

expr AggregateType::sizeVar() const {
  expr elems = numElements();
  expr sz = expr::mkUInt(0, var_bw_bits);

  for (unsigned i = 0; i < elements; ++i) {
    sz = expr::mkIf(elems.ugt(i), sz + children[i]->sizeVar(), sz);
  }
  return sz;
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

  expr r = numElementsExcludingPadding() == element_types->size();
  auto types = element_types->begin(), types_E = element_types->end();
  for (unsigned i = 0, e = children.size(); i != e && types != types_E; ++i) {
    if (!isPadding(i))
      r &= *children[i] == **types++;
  }
  return r;
}

expr AggregateType::toBV(expr e) const {
  return Type::toBV(std::move(e));
}

StateValue AggregateType::toBV(StateValue v) const {
  return v;
}

expr AggregateType::fromBV(expr e) const {
  return Type::fromBV(std::move(e));
}

StateValue AggregateType::fromBV(StateValue v) const {
  return v;
}

expr AggregateType::toInt(State &s, expr v) const {
  UNREACHABLE();
}

StateValue AggregateType::toInt(State &s, StateValue v) const {
  // structs can be empty
  if (elements == 0)
    return { expr::mkUInt(0, 1), expr::mkUInt(1, 1) };

  StateValue ret;
  for (unsigned i = 0; i < elements; ++i) {
    auto vv = children[i]->toInt(s, extract(v, i));
    ret = i == 0 ? std::move(vv) : (little_endian ? vv.concat(ret) : ret.concat(vv));
  }
  return ret;
}

expr AggregateType::fromInt(expr v) const {
  UNREACHABLE();
}

StateValue AggregateType::fromInt(StateValue v) const {
  vector<StateValue> child_vals;
  for (unsigned i = 0; i < elements; ++i)
    child_vals.emplace_back(extract(v, i, true));
  return this->aggregateVals(child_vals);
}

pair<expr, expr>
AggregateType::refines(State &src_s, State &tgt_s, const StateValue &src,
                       const StateValue &tgt) const {
  set<expr> poison, value;
  for (unsigned i = 0; i < elements; ++i) {
    auto [p, v] = children[i]->refines(src_s, tgt_s, extract(src, i),
                                       extract(tgt, i));
    poison.insert(std::move(p));
    value.insert(std::move(v));
  }
  return { expr::mk_and(poison), expr::mk_and(value) };
}

StateValue AggregateType::mkUndef(State &s) const {
  vector<StateValue> vals;
  for (unsigned i = 0; i < elements; ++i) {
    if (!isPadding(i))
      vals.emplace_back(children[i]->mkUndef(s));
  }
  return aggregateVals(vals);
}

expr AggregateType::mkInput(State &s, const char *name,
                            const ParamAttrs &attrs) const {
  UNREACHABLE();
}

unsigned AggregateType::numPointerElements() const {
  unsigned count = 0;
  for (unsigned i = 0; i < elements; ++i) {
    if (children[i]->isPtrType())
      count++;
    else if (auto aty = children[i]->getAsAggregateType())
      count += aty->numPointerElements();
  }
  return count;
}

void AggregateType::printVal(ostream &os, const State &s, const Model &m,
                             const expr &e) const {
  UNREACHABLE();
}

const AggregateType* AggregateType::getAsAggregateType() const {
  return this;
}


ArrayType::ArrayType(string &&name, unsigned elements, Type &elementTy,
                     Type *paddingTy)
  : AggregateType(std::move(name), false) {
  defined = true;
  if (paddingTy) {
    this->elements = elements * 2;
    children.resize(elements * 2, &elementTy);
    is_padding.resize(elements * 2, false);
    for (unsigned i = 1; i < elements * 2; i += 2) {
      children[i] = paddingTy;
      is_padding[i] = true;
    }
  } else {
    this->elements = elements;
    children.resize(elements, &elementTy);
    is_padding.resize(elements, false);
  }
}

bool ArrayType::isArrayType() const {
  return true;
}

void ArrayType::print(ostream &os) const {
  if (children.empty())
    os << "(empty array)";
  else {
    bool has_padding = elements > 1 && isPadding(1);
    assert(!has_padding || elements % 2 == 0);
    os << '[' << (has_padding ? elements / 2 : elements) << " x "
       << *children[0];
    if (has_padding)
       os << ", padding: " << *children[1];
    os << ']';
  }
}


VectorType::VectorType(string &&name, unsigned elements, Type &elementTy)
  : AggregateType(std::move(name), false) {
  assert(elements != 0);
  this->elements = elements;
  defined = true;
  children.resize(elements, &elementTy);
  is_padding.resize(elements, false);
}

StateValue VectorType::extract(const StateValue &vector,
                               const expr &index) const {
  auto &elementTy = *children[0];
  unsigned bw_elem = elementTy.bits();
  unsigned bw_val = bw_elem * elements;
  expr idx_v = index.zextOrTrunc(bw_val) * expr::mkUInt(bw_elem, bw_val);
  unsigned h_val = elements * bw_elem - 1;
  unsigned l_val = (elements - 1) * bw_elem;

  unsigned bw_np_elem = elementTy.np_bits();
  unsigned bw_np = bw_np_elem * elements;
  expr idx_np = index.zextOrTrunc(bw_np) * expr::mkUInt(bw_np_elem, bw_np);
  unsigned h_np = elements * bw_np_elem - 1;
  unsigned l_np = (elements - 1) * bw_np_elem;

  return elementTy.fromBV({(vector.value << idx_v).extract(h_val, l_val),
                           (vector.non_poison << idx_np).extract(h_np, l_np)});
}

StateValue VectorType::update(const StateValue &vector,
                              const StateValue &val,
                              const expr &index) const {
  auto &elementTy = *children[0];
  StateValue val_bv = elementTy.toBV(val);

  if (elements == 1)
    return val_bv;

  unsigned bw_elem = elementTy.bits();
  unsigned bw_val = bw_elem * elements;
  expr idx_v = index.zextOrTrunc(bw_val) * expr::mkUInt(bw_elem, bw_val);
  expr fill_v = expr::mkUInt(0, bw_val - bw_elem);
  expr mask_v = ~expr::mkInt(-1, bw_elem).concat(fill_v).lshr(idx_v);
  expr nv_shifted = val_bv.value.concat(fill_v).lshr(idx_v);

  unsigned bw_np_elem = elementTy.np_bits();
  unsigned bw_np = bw_np_elem * elements;
  expr idx_np = index.zextOrTrunc(bw_np) * expr::mkUInt(bw_np_elem, bw_np);
  expr fill_np = expr::mkUInt(0, bw_np - bw_np_elem);
  expr mask_np = ~expr::mkInt(-1, bw_np_elem).concat(fill_np).lshr(idx_np);
  expr np_shifted = val_bv.non_poison.concat(fill_np).lshr(idx_np);

  return fromBV({ (vector.value & mask_v) | nv_shifted,
                  (vector.non_poison & mask_np) | np_shifted});
}

unsigned VectorType::np_bits() const {
  if (getChild(0).isPtrType())
    return elements;
  return Type::np_bits();
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

expr VectorType::scalarSize() const {
  return children[0]->sizeVar();
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


StructType::StructType(string &&name, vector<Type*> &&children,
                       vector<bool> &&is_padding)
  : AggregateType(std::move(name), std::move(children), std::move(is_padding)) {
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

#define DISPATCH_EXPR(call)                                               \
  expr ret;                                                               \
  if (i)                                                                  \
    ret = i->call;                                                        \
  if (f)                                                                  \
    ret = ret.isValid() ? expr::mkIf(isFloat(), f->call, ret) : f->call;  \
  if (p)                                                                  \
    ret = ret.isValid() ? expr::mkIf(isPtr(), p->call, ret) : p->call;    \
  if (a)                                                                  \
    ret = ret.isValid() ? expr::mkIf(isArray(), a->call, ret) : a->call;  \
  if (v)                                                                  \
    ret = ret.isValid() ? expr::mkIf(isVector(), v->call, ret) : v->call; \
  if (s)                                                                  \
    ret = ret.isValid() ? expr::mkIf(isStruct(), s->call, ret) : s->call; \
  return ret

unsigned SymbolicType::bits() const {
  DISPATCH(bits(), UNREACHABLE());
}

unsigned SymbolicType::np_bits() const {
  DISPATCH(np_bits(), UNREACHABLE());
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

expr SymbolicType::sizeVar() const {
  DISPATCH_EXPR(sizeVar());
}

expr SymbolicType::scalarSize() const {
  DISPATCH_EXPR(scalarSize());
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
    return std::move(c) && typeVar() == rhs->typeVar();
  }
  assert(0 && "unhandled case in SymbolicType::operator==");
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
  DISPATCH(toBV(std::move(e)), UNREACHABLE());
}

StateValue SymbolicType::toBV(StateValue val) const {
  DISPATCH(toBV(std::move(val)), UNREACHABLE());
}

expr SymbolicType::fromBV(expr e) const {
  DISPATCH(fromBV(std::move(e)), UNREACHABLE());
}

StateValue SymbolicType::fromBV(StateValue val) const {
  DISPATCH(fromBV(std::move(val)), UNREACHABLE());
}

expr SymbolicType::toInt(State &st, expr e) const {
  DISPATCH(toInt(st, std::move(e)), UNREACHABLE());
}

StateValue SymbolicType::toInt(State &st, StateValue val) const {
  DISPATCH(toInt(st, std::move(val)), UNREACHABLE());
}

expr SymbolicType::fromInt(expr e) const {
  DISPATCH(fromInt(std::move(e)), UNREACHABLE());
}

StateValue SymbolicType::fromInt(StateValue val) const {
  DISPATCH(fromInt(std::move(val)), UNREACHABLE());
}

pair<expr, expr>
SymbolicType::refines(State &src_s, State &tgt_s, const StateValue &src,
                      const StateValue &tgt) const {
  DISPATCH(refines(src_s, tgt_s, src, tgt), UNREACHABLE());
}

StateValue SymbolicType::mkUndef(State &st) const {
  DISPATCH(mkUndef(st), UNREACHABLE());
}

expr SymbolicType::mkInput(State &st, const char *name,
                           const ParamAttrs &attrs) const {
  DISPATCH(mkInput(st, name, attrs), UNREACHABLE());
}

pair<expr, expr>
SymbolicType::mkUndefInput(State &st, const ParamAttrs &attrs) const {
  DISPATCH(mkUndefInput(st, attrs), UNREACHABLE());
}

void SymbolicType::printVal(ostream &os, const State &st, const Model &m,
                            const expr &e) const {
  DISPATCH(printVal(os, st, m, e), UNREACHABLE());
}

void SymbolicType::print(ostream &os) const {
  DISPATCH(print(os), return);
}


bool hasPtr(const Type &t) {
  if (t.isPtrType())
    return true;

  if (auto agg = t.getAsAggregateType()) {
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      if (hasPtr(agg->getChild(i)))
        return true;
    }
  }
  return false;
}

bool isNonPtrVector(const Type &t) {
  auto vty = dynamic_cast<const VectorType *>(&t);
  return vty && !vty->getChild(0).isPtrType();
}

unsigned minVectorElemSize(const Type &t) {
  if (auto agg = t.getAsAggregateType()) {
    if (t.isVectorType()) {
      auto &elemTy = agg->getChild(0);
      return elemTy.isPtrType() ? IR::bits_program_pointer : elemTy.bits();
    }

    unsigned val = 0;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e;  ++i) {
      if (auto ch = minVectorElemSize(agg->getChild(i))) {
        val = val ? gcd(val, ch) : ch;
      }
    }
    return val;
  }
  return 0;
}

uint64_t getCommonAccessSize(const IR::Type &ty) {
  if (auto agg = ty.getAsAggregateType()) {
    // non-pointer vectors are stored/loaded all at once
    if (agg->isVectorType()) {
      auto &elemTy = agg->getChild(0);
      if (!elemTy.isPtrType())
        return divide_up(agg->numElementsConst() * elemTy.bits(), 8);
    }

    uint64_t sz = 1;
    for (unsigned i = 0, e = agg->numElementsConst(); i != e; ++i) {
      auto n = getCommonAccessSize(agg->getChild(i));
      sz = i == 0 ? n : gcd(sz, n);
    }
    return sz;
  }
  if (ty.isPtrType())
    return IR::bits_program_pointer / 8;
  return divide_up(ty.bits(), 8);
}
}
