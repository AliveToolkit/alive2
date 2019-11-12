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
  virtual std::vector<Value*> operands() const = 0;
  virtual void rauw(const Value &what, Value &with) = 0;
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
            SMul_Overflow, UMul_Overflow,
            FAdd, FSub, FMul, FDiv, FRem,
            And, Or, Xor, Cttz, Ctlz  };
  enum Flags { None = 0, NSW = 1 << 0, NUW = 1 << 1, Exact = 1 << 2,
               NNaN = 1 << 3, NInf = 1 << 4, NSZ = 1 << 5, ARCP = 1 << 6,
               Contract = 1 << 7, Reassoc = 1 << 8,
               FastMath = NNaN | NInf | NSZ | ARCP | Contract | Reassoc };

private:
  Value *lhs, *rhs;
  Op op;
  unsigned flags;

public:
  BinOp(Type &type, std::string &&name, Value &lhs, Value &rhs, Op op,
        unsigned flags = 0);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class UnaryOp final : public Instr {
public:
  enum Op { Copy, BitReverse, BSwap, Ctpop, FNeg };

private:
  Value *val;
  Op op;

public:
  UnaryOp(Type &type, std::string &&name, Value &val, Op op)
    : Instr(type, std::move(name)), val(&val), op(op) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class TernaryOp final : public Instr {
public:
  enum Op { FShl, FShr };

private:
  Value *a, *b, *c;
  Op op;

public:
  TernaryOp(Type &type, std::string &&name, Value &a, Value &b, Value &c, Op op)
      : Instr(type, std::move(name)), a(&a), b(&b), c(&c), op(op) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class ConversionOp final : public Instr {
public:
  enum Op { SExt, ZExt, Trunc, BitCast, SIntToFP, UIntToFP, FPToSInt, FPToUInt,
            Ptr2Int, Int2Ptr };

private:
  Value *val;
  Op op;

public:
  ConversionOp(Type &type, std::string &&name, Value &val, Op op)
    : Instr(type, std::move(name)), val(&val), op(op) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Select final : public Instr {
  Value *cond, *a, *b;
public:
  Select(Type &type, std::string &&name, Value &cond, Value &a, Value &b)
    : Instr(type, std::move(name)), cond(&cond), a(&a), b(&b) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class ExtractValue final : public Instr {
  Value *val;
  std::vector<unsigned> idxs;
public:
  ExtractValue(Type &type, std::string &&name, Value &val)
    : Instr(type, std::move(name)), val(&val) {}
  void addIdx(unsigned idx);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class FnCall final : public Instr {
  std::string fnName;
  std::vector<Value*> args;
  bool valid;
public:
  FnCall(Type &type, std::string &&name, std::string &&fnName,
         bool valid = true)
    : Instr(type, std::move(name)), fnName(std::move(fnName)), valid(valid) {}
  void addArg(Value &arg);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class ICmp final : public Instr {
public:
  enum Cond { EQ, NE, SLE, SLT, SGE, SGT, ULE, ULT, UGE, UGT, Any };

private:
  Value *a, *b;
  std::string cond_name;
  Cond cond;
  bool defined;
  smt::expr cond_var() const;

public:
  ICmp(Type &type, std::string &&name, Cond cond, Value &a, Value &b);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class FCmp final : public Instr {
public:
  enum Cond { OEQ, OGT, OGE, OLT, OLE, ONE, ORD,
              UEQ, UGT, UGE, ULT, ULE, UNE, UNO };
  enum Flags { None = 0, NNaN = 1, NInf = 2, Reassoc = 4 };

private:
  Value *a, *b;
  Cond cond;
  unsigned flags = 0;

public:
  FCmp(Type &type, std::string &&name, Cond cond, Value &a, Value &b,
       unsigned flags)
    : Instr(type, move(name)), a(&a), b(&b), cond(cond), flags(flags) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Freeze final : public Instr {
  Value *val;
public:
  Freeze(Type &type, std::string &&name, Value &val)
    : Instr(type, std::move(name)), val(&val) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Phi final : public Instr {
  std::vector<std::pair<Value*, std::string>> values;
public:
  Phi(Type &type, std::string &&name) : Instr(type, std::move(name)) {}

  void addValue(Value &val, std::string &&BB_name);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
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
  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Switch final : public JumpInstr {
  Value *value;
  const BasicBlock &default_target;
  std::vector<std::pair<Value*, const BasicBlock&>> targets;

public:
  Switch(Value &value, const BasicBlock &default_target)
    : JumpInstr(Type::voidTy, "switch"), value(&value),
      default_target(default_target) {}

  void addTarget(Value &val, const BasicBlock &target);

  auto getNumTargets() const { return targets.size(); }
  auto& getTarget(unsigned i) const { return targets[i]; }
  auto& getDefault() const { return default_target; }

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Return final : public Instr {
  Value *val;
public:
  Return(Type &type, Value &val) : Instr(type, "return"), val(&val) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Assume final : public Instr {
  Value *cond;
  bool if_non_poison; /// cond only needs to hold if non-poison
public:
  Assume(Value &cond, bool if_non_poison)
    : Instr(Type::voidTy, "assume"), cond(&cond),
      if_non_poison(if_non_poison) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Alloc final : public Instr {
  Value *size;
  unsigned align;
public:
  Alloc(Type &type, std::string &&name, Value &size, unsigned align)
    : Instr(type, std::move(name)), size(&size), align(align) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Malloc final : public Instr {
  Value *size;
  // Is this malloc (or equivalent operation, like new()) never returning
  // null?
  bool isNonNull;
public:
  Malloc(Type &type, std::string &&name, Value &size, bool isNonNull)
    : Instr(type, std::move(name)), size(&size), isNonNull(isNonNull) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Calloc final : public Instr {
  Value *num, *size;
public:
  Calloc(Type &type, std::string &&name, Value &num, Value &size)
    : Instr(type, std::move(name)), num(&num), size(&size) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Free final : public Instr {
  Value *ptr;
public:
  Free(Value &ptr) : Instr(Type::voidTy, "free"), ptr(&ptr) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class GEP final : public Instr {
  Value *ptr;
  std::vector<std::pair<unsigned, Value*>> idxs;
  bool inbounds;
public:
  GEP(Type &type, std::string &&name, Value &ptr, bool inbounds)
    : Instr(type, std::move(name)), ptr(&ptr), inbounds(inbounds) {}

  void addIdx(unsigned obj_size, Value &idx);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Load final : public Instr {
  Value *ptr;
  unsigned align;
public:
  Load(Type &type, std::string &&name, Value &ptr, unsigned align)
    : Instr(type, std::move(name)), ptr(&ptr), align(align) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Store final : public Instr {
  Value *ptr, *val;
  unsigned align;
public:
  Store(Value &ptr, Value &val, unsigned align)
    : Instr(Type::voidTy, "store"), ptr(&ptr), val(&val), align(align) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Memset final : public Instr {
  Value *ptr, *val, *bytes;
  unsigned align;
public:
  Memset(Value &ptr, Value &val, Value &bytes, unsigned align)
    : Instr(Type::voidTy, "memset"), ptr(&ptr), val(&val), bytes(&bytes),
            align(align) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Memcpy final : public Instr {
  Value *dst, *src, *bytes;
  unsigned align_dst, align_src;
  bool move;
public:
  Memcpy(Value &dst, Value &src, Value &bytes,
         unsigned align_dst, unsigned align_src, bool move)
    : Instr(Type::voidTy, "memcpy"), dst(&dst), src(&src), bytes(&bytes),
            align_dst(align_dst), align_src(align_src), move(move) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class ExtractElement final : public Instr {
  Value *v, *idx;
public:
  ExtractElement(Type &type, std::string &&name, Value &v, Value &idx)
    : Instr(type, move(name)), v(&v), idx(&idx) {}
  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class InsertElement final : public Instr {
  Value *v, *e, *idx;
public:
  InsertElement(Type &type, std::string &&name, Value &v, Value &e, Value &idx)
    : Instr(type, move(name)), v(&v), e(&e), idx(&idx) {}
  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class ShuffleVector final : public Instr {
  Value *v1, *v2, *mask;
public:
  ShuffleVector(Type &type, std::string &&name, Value &v1, Value &v2, Value &m)
    : Instr(type, move(name)), v1(&v1), v2(&v2), mask(&m) {}
  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};

}
