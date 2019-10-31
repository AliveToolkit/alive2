#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/type.h"
#include "smt/expr.h"
#include <ostream>
#include <string>
#include <variant>

namespace smt { class Model; }

namespace IR {

class VoidValue;


class Value {
  Type &type;
  std::string name;

protected:
  Value(Type &type, std::string &&name)
    : type(type), name(std::move(name)) {}

  void setName(std::string &&str) { name = std::move(str); }
  static std::string fresh_id();

public:
  auto bits() const { return type.bits(); }
  auto& getName() const { return name; }
  auto& getType() const { return type; }

  virtual void print(std::ostream &os) const = 0;
  virtual StateValue toSMT(State &s) const = 0;
  virtual smt::expr getTypeConstraints() const;
  void fixupTypes(const smt::Model &m);

  static VoidValue voidVal;

  static void reset_gbl_id();

  friend std::ostream& operator<<(std::ostream &os, const Value &val);

  virtual ~Value() {}
};


class UndefValue final : public Value {
public:
  UndefValue(Type &type) : Value(type, "undef") {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;

  static std::string getFreshName();
};


class PoisonValue final : public Value {
public:
  PoisonValue(Type &type) : Value(type, "poison") {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
};


class VoidValue final : public Value {
public:
  VoidValue() : Value(Type::voidTy, "void") {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
};


class NullPointerValue final : public Value {
public:
  NullPointerValue(Type &type) : Value(type, "null") {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
};


class GlobalVariable final : public Value {
  // The size of this global variable (in bytes)
  int allocsize;
  // Alignment of this global variable
  int align;
  // Initial value stored at this global variable.
  // This is initialized only if the global is constant.
  // If it has no initial value, this is nullptr.
  Value *initval;
  // Is this global variable constant?
  // initval can be null even if isconst is true if this is an external const.
  bool isconst;
public:
  GlobalVariable(Type &type, std::string &&name, int allocsize, int align,
                 Value *initval, bool isconst) :
    Value(type, std::move(name)), allocsize(allocsize), align(align),
    initval(initval), isconst(isconst) {}
  int size() const { return allocsize; }
  int isConst() const { return isconst; }
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
};


class Input final : public Value {
  std::string smt_name;
public:
  Input(Type &type, std::string &&name);
  void copySMTName(const Input &other);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTyVar() const;
};

}
