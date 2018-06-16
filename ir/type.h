#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/expr.h"
#include <memory>
#include <string>
#include <ostream>

namespace smt { class Model; }

namespace IR {

class Type {
  // name of associated operand
  std::string opname;

protected:
  smt::expr var(const char *var, unsigned bits) const;
  smt::expr typeVar() const;
  smt::expr is(unsigned t) const;
  smt::expr isInt() const;
  smt::expr isFloat() const;
  smt::expr isPtr() const;
  smt::expr isArray() const;
  smt::expr isVector() const;

public:
  virtual void setName(const std::string &name);
  virtual unsigned bits() const;

  virtual smt::expr getTypeConstraints() const = 0;
  smt::expr sizeVar() const;
  smt::expr operator==(const Type &rhs) const;
  virtual void fixup(const smt::Model &m) = 0;

  virtual void enforceIntType();
  virtual void enforceIntOrPtrOrVectorType();

  virtual std::unique_ptr<Type> dup() const = 0;
  virtual void print(std::ostream &os) const = 0;
  std::string toString() const;

  virtual ~Type();
};


class VoidType final : public Type {
public:
  VoidType() {}
  smt::expr getTypeConstraints() const override;
  void fixup(const smt::Model &m) override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};


class IntType final : public Type {
  unsigned bitwidth = 0;
  bool defined = false;

public:
  IntType() {}
  IntType(unsigned bitwidth) : bitwidth(bitwidth), defined(true) {}
  unsigned bits() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const IntType &rhs) const;
  void fixup(const smt::Model &m) override;
  void enforceIntType() override;
  void enforceIntOrPtrOrVectorType() override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};


class FloatType final : public Type {
public:
  FloatType() {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const FloatType &rhs) const;
  void fixup(const smt::Model &m) override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};


class PtrType final : public Type {
public:
  PtrType() {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const PtrType &rhs) const;
  void fixup(const smt::Model &m) override;
  void enforceIntOrPtrOrVectorType() override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};


class ArrayType final : public Type {
public:
  ArrayType() {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const ArrayType &rhs) const;
  void fixup(const smt::Model &m) override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};


class VectorType final : public Type {
public:
  VectorType() {}
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const VectorType &rhs) const;
  void fixup(const smt::Model &m) override;
  void enforceIntOrPtrOrVectorType() override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};


class SymbolicType final : public Type {
public:
  enum TypeNum { Int, Float, Ptr, Array, Vector, Undefined };

private:
  std::string name;
  TypeNum typ = Undefined;
  unsigned enabled = (1 << Int) | (1 << Float) | (1 << Ptr) | (1 << Array) |
                     (1 << Vector);
  IntType i;
  FloatType f;
  PtrType p;
  ArrayType a;
  VectorType v;

  smt::expr isInt() const;
  smt::expr isFloat() const;
  smt::expr isPtr() const;
  smt::expr isArray() const;
  smt::expr isVector() const;

public:
  SymbolicType() {}
  SymbolicType(std::string &&name) : name(std::move(name)) {}
  void setName(const std::string &name) override;
  unsigned bits() const override;
  smt::expr getTypeConstraints() const override;
  smt::expr operator==(const Type &rhs) const;
  void fixup(const smt::Model &m) override;
  void enforceIntType() override;
  void enforceIntOrPtrOrVectorType() override;
  std::unique_ptr<Type> dup() const override;
  void print(std::ostream &os) const override;
};

}
