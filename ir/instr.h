#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"

namespace IR {

class Instr : public Value {
protected:
  Instr(std::unique_ptr<Type> &&type, std::string &&name)
    : Value(std::move(type), std::move(name)) {}
};


class BinOp final : public Instr {
public:
  enum Op { Add, Sub, Mul, SDiv, UDiv, Shl, AShr, LShr };
  enum Flags { None = 0, NSW = 1, NUW = 2, NSWNUW = 3, Exact = 4 };

private:
  Value &lhs, &rhs;
  Op op;
  Flags flags;

public:
  BinOp(std::unique_ptr<Type> &&type, std::string &&name, Value &lhs,
        Value &rhs, Op op, Flags flags = None);

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
  ~BinOp();
};


class Unreachable final : public Instr {
public:
  Unreachable() : Instr(std::make_unique<VoidType>(), "") {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
  ~Unreachable();
};

}
