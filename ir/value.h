#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/state.h"
#include "ir/type.h"
#include "smt/expr.h"
#include <ostream>
#include <string>

namespace smt { class Model; }

namespace IR {

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
  virtual void fixupTypes(const smt::Model &m);

  static void reset_gbl_id();

  friend std::ostream& operator<<(std::ostream &os, const Value &val);

  virtual ~Value() {}
};


class IntConst final : public Value {
  int64_t val;

public:
  IntConst(Type &type, int64_t val);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
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


class Input final : public Value {
public:
  Input(Type &type, std::string &&name) :
    Value(type, std::move(name)) {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTyVar() const;
};

}
