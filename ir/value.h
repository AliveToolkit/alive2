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

public:
  auto bits() const { return type.bits(); }
  auto& getName() const { return name; }
  auto& getType() const { return type; }
  bool isVoid() const;

  virtual void print(std::ostream &os) const = 0;
  virtual StateValue toSMT(State &s) const = 0;
  virtual smt::expr getTypeConstraints() const;
  void fixupTypes(const smt::Model &m);

  static VoidValue voidVal;

  friend std::ostream& operator<<(std::ostream &os, const Value &val);

  virtual ~Value() {}
};


class UndefValue final : public Value {
public:
  UndefValue(Type &type) : Value(type, "undef") {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
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
  uint64_t allocsize;
  unsigned align;
  bool isconst;
public:
  GlobalVariable(Type &type, std::string &&name, uint64_t allocsize,
                 unsigned align, bool isconst) :
    Value(type, std::move(name)), allocsize(allocsize), align(align),
    isconst(isconst) {}
  uint64_t size() const { return allocsize; }
  bool isConst() const { return isconst; }
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
};


class AggregateValue final : public Value {
  std::vector<Value*> vals;
public:
  AggregateValue(Type &type, std::vector<Value*> &&vals);
  auto& getVals() const { return vals; }
  smt::expr getTypeConstraints() const override;
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
