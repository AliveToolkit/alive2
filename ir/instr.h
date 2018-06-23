#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"

namespace IR {

class Instr : public Value {
protected:
  Instr(std::unique_ptr<Type> &&type, std::string &&name,
        bool mk_unique_name = false)
    : Value(std::move(type), std::move(name), mk_unique_name) {}
};


class BinOp final : public Instr {
public:
  enum Op { Add, Sub, Mul, SDiv, UDiv, SRem, URem, Shl, AShr, LShr,
            And, Or, Xor };
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
};


class ConversionOp final : public Instr {
public:
  enum Op { SExt, ZExt, Trunc };

private:
  Value &val;
  Op op;

public:
  ConversionOp(std::unique_ptr<Type> &&type, std::string &&name, Value &val,
               Op op);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class Select final : public Instr {
  Value &cond, &a, &b;
public:
  Select(std::unique_ptr<Type> &&type, std::string &&name, Value &cond,
         Value &a, Value &b);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class ICmp final : public Instr {
public:
  enum Cond { EQ, NE, SLE, SLT, SGE, SGT, ULE, ULT, UGE, UGT, Any };

private:
  Value &a, &b;
  smt::expr cond_var;
  Cond cond;

public:
  ICmp(std::string &&name, Cond cond, Value &a, Value &b);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
  void fixupTypes(const smt::Model &m) override;
};


class Freeze final : public Instr {
  Value &val;
public:
  Freeze(std::unique_ptr<Type> &&type, std::string &&name, Value &val) :
    Instr(std::move(type), std::move(name)), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class CopyOp final : public Instr {
  Value &val;
public:
  CopyOp(std::unique_ptr<Type> &&type, std::string &&name, Value &val) :
    Instr(std::move(type), std::move(name)), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class Return final : public Instr {
  Value &val;
public:
  Return(std::unique_ptr<Type> &&type, Value &val) :
    Instr(std::move(type), "return", true), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};


class Unreachable final : public Instr {
public:
  Unreachable() : Instr(std::make_unique<VoidType>(), "") {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints() const override;
};

}
