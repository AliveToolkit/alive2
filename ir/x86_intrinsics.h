#pragma once

#include "ir/instr.h"

namespace IR {
class X86IntrinBinOp final : public Instr {
public:
  enum Op {
#define PROCESS(NAME, A, B, C, D, E, F) NAME,
#include "x86_intrinsics_binop.inc"
#undef PROCESS
  };

private:
  Value *a, *b;
  Op op;

public:
  X86IntrinBinOp(Type &type, std::string &&name, Value &a, Value &b, Op op)
      : Instr(type, std::move(name)), a(&a), b(&b), op(op) {}
  std::vector<Value *> operands() const override;
  bool propagatesPoison() const override;
  bool hasSideEffects() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(Function &f,
                             const std::string &suffix) const override;
};

class X86IntrinTerOp final : public Instr {
public:
  enum Op {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) NAME,
#include "x86_intrinsics_terop.inc"
#undef PROCESS
  };

private:
  Value *a, *b, *c;
  Op op;

public:
  X86IntrinTerOp(Type &type, std::string &&name, Value &a, Value &b, Value &c,
                 Op op)
      : Instr(type, std::move(name)), a(&a), b(&b), c(&c), op(op) {}
  std::vector<Value *> operands() const override;
  bool propagatesPoison() const override;
  bool hasSideEffects() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(Function &f,
                             const std::string &suffix) const override;
};

class X86IntrinQuadOp final : public Instr {
public:
  enum Op {
#define PROCESS(NAME) NAME,
#include "x86_intrinsics_quadop.inc"
#undef PROCESS
  };

private:
  Value *a, *b, *c, *d;
  Op op;

public:
  X86IntrinQuadOp(Type &type, std::string &&name, Value &a, Value &b, Value &c,
                  Value &d, Op op)
      : Instr(type, std::move(name)), a(&a), b(&b), c(&c), d(&d), op(op) {}
  std::vector<Value *> operands() const override;
  bool propagatesPoison() const override;
  bool hasSideEffects() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(Function &f,
                             const std::string &suffix) const override;
};

} // namespace IR
