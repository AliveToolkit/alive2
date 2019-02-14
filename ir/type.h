#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <cassert>
#include <initializer_list>
#include <memory>
#include <string>
#include <ostream>

namespace smt { class Model; }

namespace IR {

class VoidType;

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
  smt::expr isAggregate() const;

public:
  Type(std::string &&name) : name(std::move(name)) {}
  virtual unsigned bits() const;

  virtual smt::expr getTypeConstraints() const = 0;
  virtual smt::expr sizeVar() const;
  smt::expr operator==(const Type &rhs) const;
  virtual void fixup(const smt::Model &m) = 0;

  virtual smt::expr enforceIntType() const;
  virtual smt::expr enforceIntOrVectorType() const;
  virtual smt::expr enforceIntOrPtrOrVectorType() const;
  virtual smt::expr enforceAggregateType() const;

  virtual void print(std::ostream &os) const = 0;
  friend std::ostream& operator<<(std::ostream &os, const Type &t);
  std::string toString() const;

  static VoidType voidTy;

  virtual ~Type();
};


class VoidType final : public Type {
public:
  VoidType() : Type("void") {}
  smt::expr getTypeConstraints() const override;
  void fixup(const smt::Model &m) override;
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
  smt::expr getTypeConstraints() const override;
  smt::expr sizeVar() const override;
  smt::expr operator==(const IntType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceIntType() const override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  void print(std::ostream &os) const override;
};


class FloatType final : public Type {
public:
  FloatType(std::string &&name) : Type(std::move(name)) {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const FloatType &rhs) const;
  void fixup(const smt::Model &m) override;
  void print(std::ostream &os) const override;
};


class PtrType final : public Type {
public:
  PtrType(std::string &&name) : Type(std::move(name)) {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const PtrType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  void print(std::ostream &os) const override;
};


class ArrayType final : public Type {
public:
  ArrayType(std::string &&name) : Type(std::move(name)) {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const ArrayType &rhs) const;
  void fixup(const smt::Model &m) override;
  void print(std::ostream &os) const override;
};


class VectorType final : public Type {
public:
  VectorType(std::string &&name) : Type(std::move(name)) {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const VectorType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  void print(std::ostream &os) const override;
};


// Currently only supports aggregate type with two children
// of int type.
// Eg: {ix, i1}. Enforced in getTypeConstraints()
class AggregateType final : public Type {
  std::vector<Type*> childrenType;

public:
  AggregateType(std::string &&name) : Type(std::move(name)) {}
  AggregateType(std::string &&name, std::initializer_list<Type*> types)
    : Type(std::move(name)), childrenType(types) {
    assert(childrenType.size() > 0);
  }

  unsigned bits() const override;
  smt::expr enforceAggregateType() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const AggregateType &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  void print(std::ostream &os) const override;

  unsigned getChildrenSize() const { return childrenType.size(); }
  // constraint enforcing type to be one of the aggregate's children
  smt::expr getChildrenConstraints(Type &type) const;
  // Extracts the type located at `b` from `a`.
  smt::expr extract(const smt::expr &a, const smt::expr &b) const;
};


class SymbolicType final : public Type {
public:
  enum TypeNum { Int, Float, Ptr, Array, Vector, Aggregate, Undefined };

private:
  TypeNum typ = Undefined;
  IntType i;
  FloatType f;
  PtrType p;
  ArrayType a;
  VectorType v;
  AggregateType ag;
  bool named;

public:
  SymbolicType(std::string &&name, bool named = false);
  unsigned bits() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const Type &rhs) const;
  void fixup(const smt::Model &m) override;
  smt::expr enforceIntType() const override;
  smt::expr enforceIntOrVectorType() const override;
  smt::expr enforceIntOrPtrOrVectorType() const override;
  void print(std::ostream &os) const override;
};

}
