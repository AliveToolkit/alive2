#pragma once

#include "ir/instr.h"

namespace IR {
class FakeShuffle final : public Instr {
  Value *v1, *v2, *mask;

public:
  FakeShuffle(Type &type, std::string &&name, Value &v1, Value &v2, Value &mask)
      : Instr(type, std::move(name)), v1(&v1), v2(&v2), mask(&mask) {}
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

class X86IntrinBinOp final : public Instr {
public:
  static constexpr unsigned numOfX86Intrinsics = 135;
  enum Op {
#define PROCESS(NAME, A, B, C, D, E, F) NAME,
#include "x86_intrinsics_binop.inc"
#undef PROCESS
  };

  // the shape of a vector is stored as <# of lanes, element bits>
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_op0 = {
#define PROCESS(NAME, A, B, C, D, E, F) std::make_pair(C, D),
#include "x86_intrinsics_binop.inc"
#undef PROCESS
      };
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_op1 = {
#define PROCESS(NAME, A, B, C, D, E, F) std::make_pair(E, F),
#include "x86_intrinsics_binop.inc"
#undef PROCESS
      };
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_ret = {
#define PROCESS(NAME, A, B, C, D, E, F) std::make_pair(A, B),
#include "x86_intrinsics_binop.inc"
#undef PROCESS
      };
  static constexpr std::array<unsigned, numOfX86Intrinsics> ret_width = {
#define PROCESS(NAME, A, B, C, D, E, F) A *B,
#include "x86_intrinsics_binop.inc"
#undef PROCESS
  };

private:
  Value *a, *b;
  Op op;

public:
  static unsigned getRetWidth(Op op) {
    return ret_width[op];
  }
  X86IntrinBinOp(Type &type, std::string &&name, Value &a, Value &b, Op op)
      : Instr(type, std::move(name)), a(&a), b(&b), op(op) {}
  std::vector<Value *> operands() const override;
  bool propagatesPoison() const override;
  bool hasSideEffects() const override;
  void rauw(const Value &what, Value &with) override;
  static std::string getOpName(Op op);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(Function &f,
                             const std::string &suffix) const override;
};

class X86IntrinTerOp final : public Instr {
public:
  static constexpr unsigned numOfX86Intrinsics = 1;
  enum Op {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) NAME,
#include "x86_intrinsics_terop.inc"
#undef PROCESS
  };

  // the shape of a vector is stored as <# of lanes, element bits>
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_op0 = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(C, D),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
      };
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_op1 = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(E, F),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
      };
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_op2 = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(G, H),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
      };
  static constexpr std::array<std::pair<unsigned, unsigned>, numOfX86Intrinsics>
      shape_ret = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) std::make_pair(A, B),
#include "x86_intrinsics_terop.inc"
#undef PROCESS
      };
  static constexpr std::array<unsigned, numOfX86Intrinsics> ret_width = {
#define PROCESS(NAME, A, B, C, D, E, F, G, H) A *B,
#include "x86_intrinsics_terop.inc"
#undef PROCESS
  };

private:
  Value *a, *b, *c;
  Op op;

public:
  static unsigned getRetWidth(Op op) {
    return ret_width[op];
  }
  X86IntrinTerOp(Type &type, std::string &&name, Value &a, Value &b, Value &c,
                 Op op)
      : Instr(type, std::move(name)), a(&a), b(&b), c(&c), op(op) {}
  std::vector<Value *> operands() const override;
  bool propagatesPoison() const override;
  bool hasSideEffects() const override;
  void rauw(const Value &what, Value &with) override;
  static std::string getOpName(Op op);
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(Function &f,
                             const std::string &suffix) const override;
};
} // namespace IR
