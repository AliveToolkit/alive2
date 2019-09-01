#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace smt { class Model; }

namespace IR {

class FloatType;
class StructType;
class VoidType;
class State;

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
  virtual smt::expr getDummyValue() const = 0;

  virtual smt::expr getTypeConstraints() const = 0;
  virtual smt::expr sizeVar() const;
  smt::expr operator==(const Type &rhs) const;
  smt::expr sameType(const Type &rhs) const;
  virtual void fixup(const smt::Model &m) = 0;

  virtual bool isIntType() const;
  virtual bool isFloatType() const;
  virtual bool isPtrType() const;

  virtual smt::expr enforceIntType(unsigned bits = 0) const;
  virtual smt::expr enforceIntOrVectorType() const;
  smt::expr enforceIntOrPtrType() const;
  virtual smt::expr enforceIntOrPtrOrVectorType() const;
  virtual smt::expr enforcePtrType() const;
  virtual smt::expr enforceStructType() const;
  virtual smt::expr enforceAggregateType(
    std::vector<Type*> *element_types = nullptr) const;
  virtual smt::expr enforceFloatType() const;

  virtual const StructType* getAsStructType() const;
  virtual const FloatType* getAsFloatType() const;

  virtual smt::expr toBV(smt::expr e) const;

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
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  void fixup(const smt::Model &m) override;
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
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr sizeVar() const override;
  smt::expr operator==(const IntType &rhs) const;
  smt::expr sameType(const IntType &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isIntType() const override;
  smt::expr enforceIntType(unsigned bits = 0) const override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
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
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr sizeVar() const override;
  smt::expr operator==(const FloatType &rhs) const;
  smt::expr sameType(const FloatType &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isFloatType() const override;
  smt::expr enforceFloatType() const override;
  const FloatType* getAsFloatType() const override;
  smt::expr toBV(smt::expr e) const override;
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
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const PtrType &rhs) const;
  smt::expr sameType(const PtrType &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isPtrType() const override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  smt::expr enforcePtrType() const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


class ArrayType final : public Type {
public:
  ArrayType(std::string &&name) : Type(std::move(name)) {}
  unsigned bits() const override;
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const ArrayType &rhs) const;
  smt::expr sameType(const ArrayType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceAggregateType(
    std::vector<Type*> *element_types) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


class SymbolicType;

class VectorType final : public Type {
  std::unique_ptr<SymbolicType> sym;
  Type &elementTy;
  unsigned elements;
  bool defined = false;
public:
  VectorType(std::string &&name, unsigned elements, Type &elementTy);
  VectorType(std::string &&name);

  smt::expr numElements() const;
  smt::expr extract(const smt::expr &val, const smt::expr &index) const;

  unsigned bits() const override;
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const VectorType &rhs) const;
  smt::expr sameType(const VectorType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  smt::expr enforceAggregateType(
    std::vector<Type*> *element_types) const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};


class StructType final : public Type {
  const std::vector<Type*> children;

public:
  StructType(std::string &&name) : Type(std::move(name)) {}
  StructType(std::string &&name, std::vector<Type*> &&children)
    : Type(std::move(name)), children(std::move(children)) {}

  unsigned bits() const override;
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const StructType &rhs) const;
  smt::expr sameType(const StructType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceStructType() const override;
  smt::expr enforceAggregateType(
    std::vector<Type*> *element_types) const override;
  const StructType* getAsStructType() const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;

  smt::expr numElements() const;
  Type& getChild(unsigned index) const;
  // Extracts the type located at \p index from \p struct_val
  smt::expr extract(const smt::expr &struct_val, unsigned index) const;
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
  smt::expr getDummyValue() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const Type &rhs) const;
  smt::expr sameType(const Type &rhs) const;
  void fixup(const smt::Model &m) override;
  bool isIntType() const override;
  bool isFloatType() const override;
  bool isPtrType() const override;
  smt::expr enforceIntType(unsigned bits = 0) const override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  smt::expr enforcePtrType() const override;
  smt::expr enforceStructType() const override;
  smt::expr enforceAggregateType(
    std::vector<Type*> *element_types) const override;
  smt::expr enforceFloatType() const override;
  const StructType* getAsStructType() const override;
  const FloatType* getAsFloatType() const override;
  std::pair<smt::expr, std::vector<smt::expr>>
    mkInput(State &s, const char *name) const override;
  void printVal(std::ostream &os, State &s, const smt::expr &e) const override;
  void print(std::ostream &os) const override;
};

}
