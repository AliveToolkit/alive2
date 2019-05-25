#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/value.h"
#include <string>
#include <utility>
#include <vector>

namespace IR {

class Function;


class Instr : public Value {
protected:
  Instr(Type &type, std::string &&name) : Value(type, std::move(name)) {}

public:
  virtual smt::expr eqType(const Instr &i) const;
  smt::expr getTypeConstraints() const override;
  virtual smt::expr getTypeConstraints(const Function &f) const = 0;
  virtual std::unique_ptr<Instr> dup(const std::string &suffix) const = 0;
};


class BinOp final : public Instr {
public:
  enum Op { Add, Sub, Mul, SDiv, UDiv, SRem, URem, Shl, AShr, LShr,
            SAdd_Sat, UAdd_Sat, SSub_Sat, USub_Sat,
            SAdd_Overflow, UAdd_Overflow, SSub_Overflow, USub_Overflow,
            SMul_Overflow, UMul_Overflow, ExtractValue,
            And, Or, Xor, Cttz, Ctlz  };
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
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class UnaryOp final : public Instr {
public:
  enum Op { Copy, BitReverse, BSwap, Ctpop };

private:
  Value &val;
  Op op;

public:
  UnaryOp(Type &type, std::string &&name, Value &val, Op op)
    : Instr(type, std::move(name)), val(val), op(op) {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};

class TernaryOp final : public Instr {
public:
  enum Op { FShl, FShr };

private:
  Value &A, &B, &C;
  Op op;

public:
  TernaryOp(Type &type, std::string &&name, Value &A, Value &B, Value &C,
            Op op)
      : Instr(type, std::move(name)), A(A), B(B), C(C), op(op) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
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
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Select final : public Instr {
  Value &cond, &a, &b;
public:
  Select(Type &type, std::string &&name, Value &cond, Value &a, Value &b)
    : Instr(type, std::move(name)), cond(cond), a(a), b(b) {}
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class FnCall final : public Instr {
  std::string fnName;
  std::vector<Value*> args;
public:
  FnCall(Type &type, std::string &&name, std::string &&fnName)
    : Instr(type, std::move(name)) , fnName(std::move(fnName)) {}
  void addArg(Value &arg);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
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
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Freeze final : public Instr {
  Value &val;
public:
  Freeze(Type &type, std::string &&name, Value &val)
    : Instr(type, std::move(name)), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Phi final : public Instr {
  std::vector<std::pair<Value&, std::string>> values;
public:
  Phi(Type &type, std::string &&name) : Instr(type, std::move(name)) {}

  void addValue(Value &val, std::string &&BB_name);

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class JumpInstr : public Instr {
public:
  JumpInstr(Type &type, std::string &&name) : Instr(type, std::move(name)) {}

  class target_iterator {
    JumpInstr *instr;
    unsigned idx;
  public:
    target_iterator() {}
    target_iterator(JumpInstr *instr, unsigned idx) : instr(instr), idx(idx) {}
    const BasicBlock& operator*() const;
    target_iterator& operator++(void) { ++idx; return *this; }
    bool operator!=(target_iterator &rhs) const { return idx != rhs.idx; }
    bool operator==(target_iterator &rhs) const { return !(*this != rhs); }
  };

  class it_helper {
    JumpInstr *instr;
  public:
    it_helper(JumpInstr *instr) : instr(instr) {}
    target_iterator begin() const { return { instr, 0 }; }
    target_iterator end() const;
  };
  it_helper targets() { return this; }
};


class Branch final : public JumpInstr {
  Value *cond = nullptr;
  const BasicBlock &dst_true, *dst_false = nullptr;
public:
  Branch(const BasicBlock &dst) : JumpInstr(Type::voidTy, "br"), dst_true(dst) {}

  Branch(Value &cond, const BasicBlock &dst_true, const BasicBlock &dst_false)
    : JumpInstr(Type::voidTy, "br"), cond(&cond), dst_true(dst_true),
    dst_false(&dst_false) {}

  auto& getTrue() const { return dst_true; }
  auto getFalse() const { return dst_false; }
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Switch final : public JumpInstr {
  Value &value;
  const BasicBlock &default_target;
  std::vector<std::pair<Value&, const BasicBlock&>> targets;

public:
  Switch(Value &value, const BasicBlock &default_target)
    : JumpInstr(Type::voidTy, "switch"), value(value),
      default_target(default_target) {}

  void addTarget(Value &val, const BasicBlock &target);

  auto getNumTargets() const { return targets.size(); }
  auto& getTarget(unsigned i) const { return targets[i]; }
  auto& getDefault() const { return default_target; }

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Return final : public Instr {
  Value &val;
public:
  Return(Type &type, Value &val) : Instr(type, "return"), val(val) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
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
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


// TODO: only supports alloca for now
class Alloc final : public Instr {
  Value &size;
  unsigned align;
public:
  Alloc(Type &type, std::string &&name, Value &size, unsigned align)
    : Instr(type, std::move(name)), size(size), align(align) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Free final : public Instr {
  Value &ptr;
public:
  Free(Value &ptr) : Instr(Type::voidTy, "free"), ptr(ptr) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Load final : public Instr {
  Value &ptr;
  unsigned align;
public:
  Load(Type &type, std::string &&name, Value &ptr, unsigned align)
    : Instr(type, std::move(name)), ptr(ptr), align(align) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Store final : public Instr {
  Value &ptr, &val;
  unsigned align;
public:
  Store(Value &ptr, Value &val, unsigned align)
    : Instr(Type::voidTy, "store"), ptr(ptr), val(val), align(align) {}

  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};

}
