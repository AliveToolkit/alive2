#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"

namespace IR {

class Function;


class Instr : public Value {
protected:
  Instr(Type &type, std::string &&name) : Value(type, std::move(name)) {}

public:
  virtual smt::expr eqType(const Instr &i) const;
  smt::expr getTypeConstraints() const override;
  virtual smt::expr getTypeConstraints(const Function &f) const = 0;
};


class BinOp final : public Instr {
public:
  enum Op { Add, Sub, Mul, SDiv, UDiv, SRem, URem, Shl, AShr, LShr,
            SAdd_Sat, UAdd_Sat, SSub_Sat, USub_Sat,
            And, Or, Xor, Cttz, Ctlz };
  enum Flags { None = 0, NSW = 1, NUW = 2, NSWNUW = 3, Exact = 4 };

private:
  Value &lhs, &rhs;
  Op op;
  Flags flags;

public:
  BinOp(Type &type, std::string &&name, Value &lhs, Value &rhs, Op op,
        Flags flags = None);

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class UnaryOp final : public Instr {
public:
  enum Op { BitReverse, BSwap, Ctpop };

private:
  Value &val;
  Op op;

public:
  UnaryOp(Type &type, std::string &&name, Value &val, Op op)
    : Instr(type, std::move(name)), val(val), op(op) {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class ConversionOp final : public Instr {
public:
  enum Op { SExt, ZExt, Trunc };

private:
  Value &val;
  Op op;

public:
  ConversionOp(Type &type, std::string &&name, Value &val, Op op)
    : Instr(type, std::move(name)), val(val), op(op) {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class Select final : public Instr {
  Value &cond, &a, &b;
public:
  Select(Type &type, std::string &&name, Value &cond, Value &a, Value &b)
    : Instr(type, move(name)), cond(cond), a(a), b(b) {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class ICmp final : public Instr {
public:
  enum Cond { EQ, NE, SLE, SLT, SGE, SGT, ULE, ULT, UGE, UGT, Any };

private:
  Value &a, &b;
  std::string cond_name;
  Cond cond;
  bool defined;
  smt::expr cond_var() const;

public:
  ICmp(Type &type, std::string &&name, Cond cond, Value &a, Value &b);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  void fixupTypes(const smt::Model &m) override;
  smt::expr eqType(const Instr &i) const override;
};


class Freeze final : public Instr {
  Value &val;
public:
  Freeze(Type &type, std::string &&name, Value &val)
    : Instr(type, std::move(name)), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class CopyOp final : public Instr {
  Value &val;
public:
  CopyOp(Type &type, std::string &&name, Value &val)
    : Instr(type, std::move(name)), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class Branch final : public Instr {
  Value *cond = nullptr;
  const BasicBlock &dst_true, *dst_false = nullptr;
public:
  Branch(const BasicBlock &dst) : Instr(Type::voidTy, "br"), dst_true(dst) {}

  Branch(Value &cond, const BasicBlock &dst_true, const BasicBlock &dst_false)
    : Instr(Type::voidTy, "br"), cond(&cond), dst_true(dst_true),
    dst_false(&dst_false) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class Switch final : public Instr {
  Value &value;
  const BasicBlock &default_target;
  std::vector<std::pair<Value&, const BasicBlock&>> targets;

public:
  Switch(Value &value, const BasicBlock &default_target)
    : Instr(Type::voidTy, "switch"), value(value),
      default_target(default_target) {}

  void addTarget(Value &val, const BasicBlock &target);

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class Return final : public Instr {
  Value &val;
public:
  Return(Type &type, Value &val) : Instr(type, "return"), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};


class Assume final : public Instr {
  Value &cond;
  bool if_non_poison; /// cond only needs to hold if non-poison
public:
  Assume(Value &cond, bool if_non_poison)
    : Instr(Type::voidTy, ""), cond(cond), if_non_poison(if_non_poison) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
};

}
