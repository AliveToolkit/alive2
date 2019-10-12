#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace smt { class Model; }

namespace IR {

class AggregateType;
class FloatType;
class StructType;
class SymbolicType;
class VoidType;
class State;
struct StateValue;

class Type {
protected:
  std::string name;
  smt::expr var(const char *var, unsigned bits) const;
  smt::expr typeVar() const;
  smt::expr is(unsigned t) const;
  smt::expr isInt() const;
  smt::expr isFloat() const;
  smt::expr isPtr() const;
  smt::expr isArray() const;
  smt::expr isVector() const;
  smt::expr isStruct() const;

public:
  Type(std::string &&name) : name(std::move(name)) {}
  virtual unsigned bits() const = 0;

  // to use when one needs the corresponding SMT type
  virtual IR::StateValue getDummyValue(bool non_poison) const = 0;

  virtual smt::expr getTypeConstraints() const = 0;
  virtual smt::expr sizeVar() const;
  smt::expr operator==(const Type &rhs) const;
  smt::expr sameType(const Type &rhs) const;
  virtual void fixup(const smt::Model &m) = 0;

  virtual bool isIntType() const;
  virtual bool isFloatType() const;
  virtual bool isPtrType() const;
  virtual bool isArrayType() const;
  virtual bool isStructType() const;
  virtual bool isVectorType() const;
  bool isAggregateType() const;

  virtual smt::expr enforceIntType(unsigned bits = 0) const;
  smt::expr enforceIntOrPtrType() const;
  virtual smt::expr enforcePtrType() const;
  virtual smt::expr enforceStructType() const;
  virtual smt::expr enforceAggregateType(
    std::vector<Type*> *element_types = nullptr) const;
  virtual smt::expr enforceFloatType() const;

  virtual smt::expr enforceVectorType(
    const std::function<smt::expr(const Type&)> &enforceElem) const;
  smt::expr enforceScalarOrVectorType(
    const std::function<smt::expr(const Type&)> &enforceElem) const;

  smt::expr enforceIntOrVectorType(unsigned bits = 0) const;
  smt::expr enforceIntOrPtrOrVectorType() const;
  smt::expr enforceFloatOrVectorType() const;

  virtual const FloatType* getAsFloatType() const;
  virtual const AggregateType* getAsAggregateType() const;
  virtual const StructType* getAsStructType() const;

  virtual smt::expr toBV(smt::expr e) const;
  virtual IR::StateValue toBV(IR::StateValue v) const;
  virtual smt::expr fromBV(smt::expr e) const;
  virtual IR::StateValue fromBV(IR::StateValue v) const;

  // returns pair of refinement constraints for <poison, !poison && value>
  virtual std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &src, const IR::StateValue &tgt) const = 0;

  virtual std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const = 0;
  virtual void printVal(std::ostream &os, State &s,
                        const smt::expr &e) const = 0;

  virtual void print(std::ostream &os) const = 0;
  friend std::ostream& operator<<(std::ostream &os, const Type &t);
  std::string toString() const;

  static VoidType voidTy;

  virtual ~Type();
};


class VoidType final : public Type {
public:
  VoidType() : Type("void") {}
  unsigned bits() const override;
  IR::StateValue getDummyValue(bool non_poison) const override;
  smt::expr getTypeConstraints() const override;
  void fixup(const smt::Model &m) override;
  std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &src,const IR::StateValue &tgt) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


class IntType final : public Type {
  unsigned bitwidth = 0;
  bool defined = false;

public:
  IntType(std::string &&name) : Type(std::move(name)) {}
  IntType(std::string &&name, unsigned bitwidth)
    : Type(std::move(name)), bitwidth(bitwidth), defined(true) {}

  unsigned bits() const override;
  IR::StateValue getDummyValue(bool non_poison) const override;
  smt::expr getTypeConstraints() const override;
  smt::expr sizeVar() const override;
  smt::expr operator==(const IntType &rhs) const;
  smt::expr sameType(const IntType &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isIntType() const override;
  smt::expr enforceIntType(unsigned bits = 0) const override;
  std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &src,const IR::StateValue &tgt) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


class FloatType final : public Type {
public:
  enum FpType {
    Half, Float, Double, Unknown
  };

private:
  FpType fpType = Unknown;
  bool defined = false;

public:
  FloatType(std::string &&name) : Type(std::move(name)) {}
  FloatType(std::string &&name, FpType fpType)
    : Type(std::move(name)), fpType(fpType), defined(true) {}
  unsigned bits() const override;
  FpType getFpType() const { return fpType; };
  IR::StateValue getDummyValue(bool non_poison) const override;
  smt::expr getTypeConstraints() const override;
  smt::expr sizeVar() const override;
  smt::expr operator==(const FloatType &rhs) const;
  smt::expr sameType(const FloatType &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isFloatType() const override;
  smt::expr enforceFloatType() const override;
  const FloatType* getAsFloatType() const override;
  smt::expr toBV(smt::expr e) const override;
  smt::expr fromBV(smt::expr e) const override;
  std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &src,const IR::StateValue &tgt) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


class PtrType final : public Type {
  unsigned addr_space = 0;
  bool defined = false;
  smt::expr ASVar() const;
public:
  // symbolic addr space ptr
  PtrType(std::string &&name) : Type(std::move(name)) {}

  PtrType(unsigned addr_space);
  unsigned bits() const override;
  IR::StateValue getDummyValue(bool non_poison) const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const PtrType &rhs) const;
  smt::expr sameType(const PtrType &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isPtrType() const override;
  smt::expr enforcePtrType() const override;
  std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &src,const IR::StateValue &tgt) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


// don't create these directly; use vectors, arrays, or structs
class AggregateType : public Type {
protected:
  std::vector<Type*> children;
  std::vector<std::unique_ptr<SymbolicType>> sym;
  unsigned elements;
  bool defined = false;

  AggregateType(std::string &&name, bool symbolic = true);
  AggregateType(std::string &&name, std::vector<Type*> &&children)
    : Type(std::move(name)), children(std::move(children)) {}

public:
  smt::expr numElements() const;
  unsigned numElementsConst() const { return elements; }

  StateValue aggregateVals(const std::vector<StateValue> &vals) const;
  IR::StateValue extract(const IR::StateValue &val, unsigned index) const;
  IR::StateValue extract(const IR::StateValue &val,
                         const smt::expr &index) const;
  Type& getChild(unsigned index) const { return *children[index]; }

  unsigned bits() const override;
  IR::StateValue getDummyValue(bool non_poison) const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const AggregateType &rhs) const;
  smt::expr sameType(const AggregateType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceAggregateType(
    std::vector<Type *> *element_types) const override;
  IR::StateValue toBV(IR::StateValue v) const override;
  IR::StateValue fromBV(IR::StateValue v) const override;
  std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &src,const IR::StateValue &tgt) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  const AggregateType* getAsAggregateType() const override;
};


class ArrayType final : public AggregateType {
public:
  ArrayType(std::string &&name) : AggregateType(std::move(name)) {}
  smt::expr getTypeConstraints() const override;
  bool isArrayType() const override;
  void print(std::ostream &os) const override;
};


class VectorType final : public AggregateType {
public:
  VectorType(std::string &&name) : AggregateType(std::move(name)) {}
  VectorType(std::string &&name, unsigned elements, Type &elementTy);

  smt::expr getTypeConstraints() const override;
  bool isVectorType() const override;
  smt::expr enforceVectorType(
    const std::function<smt::expr(const Type&)> &enforceElem) const override;
  void print(std::ostream &os) const override;
};


class StructType final : public AggregateType {
public:
  StructType(std::string &&name) : AggregateType(std::move(name)) {}
  StructType(std::string &&name, std::vector<Type*> &&children);

  bool isStructType() const override;
  smt::expr enforceStructType() const override;
  const StructType* getAsStructType() const override;
  void print(std::ostream &os) const override;
};


class SymbolicType final : public Type {
public:
  enum TypeNum { Int, Float, Ptr, Array, Vector, Struct, Undefined };

private:
  TypeNum typ = Undefined;
  std::optional<IntType> i;
  std::optional<FloatType> f;
  std::optional<PtrType> p;
  std::optional<ArrayType> a;
  std::optional<VectorType> v;
  std::optional<StructType> s;

public:
  SymbolicType(std::string &&name);
  // use mask of (1 << TypeNum)
  SymbolicType(std::string &&name, unsigned type_mask);

  unsigned bits() const override;
  IR::StateValue getDummyValue(bool non_poison) const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const Type &rhs) const;
  smt::expr sameType(const Type &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isIntType() const override;
  bool isFloatType() const override;
  bool isPtrType() const override;
  bool isArrayType() const override;
  bool isStructType() const override;
  bool isVectorType() const override;
  smt::expr enforceIntType(unsigned bits = 0) const override;
  smt::expr enforcePtrType() const override;
  smt::expr enforceStructType() const override;
  smt::expr enforceAggregateType(
    std::vector<Type*> *element_types) const override;
  smt::expr enforceFloatType() const override;
  smt::expr enforceVectorType(
    const std::function<smt::expr(const Type&)> &enforceElem) const override;
  const FloatType* getAsFloatType() const override;
  const AggregateType* getAsAggregateType() const override;
  const StructType* getAsStructType() const override;
  std::pair<smt::expr, smt::expr>
    refines(const IR::StateValue &a, const IR::StateValue &b) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};

}
