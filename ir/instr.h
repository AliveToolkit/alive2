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


struct FastMathFlags final {
  enum Flags {
    None = 0, NNaN = 1 << 0, NInf = 1 << 1, NSZ = 1 << 2, ARCP = 1 << 3,
    Contract = 1 << 4, Reassoc = 1 << 5, AFN = 1 << 6,
    FastMath = NNaN | NInf | NSZ | ARCP | Contract | Reassoc | AFN
  };
  unsigned flags = None;

  bool isNone() const { return flags == None; }
  friend std::ostream& operator<<(std::ostream &os, const FastMathFlags &fm);
};


class BinOp final : public Instr {
public:
  enum Op { Add, Sub, Mul, SDiv, UDiv, SRem, URem, Shl, AShr, LShr,
            SAdd_Sat, UAdd_Sat, SSub_Sat, USub_Sat,
            SAdd_Overflow, UAdd_Overflow, SSub_Overflow, USub_Overflow,
            SMul_Overflow, UMul_Overflow,
            FAdd, FSub, FMul, FDiv, FRem, FMax, FMin,
            And, Or, Xor, Cttz, Ctlz  };
  enum Flags { None = 0, NSW = 1 << 0, NUW = 1 << 1, Exact = 1 << 2 };

private:
  Value *lhs, *rhs;
  Op op;
  unsigned flags;
  FastMathFlags fmath;

public:
  BinOp(Type &type, std::string &&name, Value &lhs, Value &rhs, Op op,
        unsigned flags = 0, FastMathFlags fmath = {});

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class UnaryOp final : public Instr {
public:
  enum Op { Copy, BitReverse, BSwap, Ctpop, IsConstant, FNeg };

private:
  Value *val;
  Op op;
  FastMathFlags fmath;

public:
  UnaryOp(Type &type, std::string &&name, Value &val, Op op,
          FastMathFlags fmath = {})
    : Instr(type, std::move(name)), val(&val), op(op), fmath(fmath) {}

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class TernaryOp final : public Instr {
public:
  enum Op { FShl, FShr, FMA };

private:
  Value *a, *b, *c;
  Op op;
  FastMathFlags fmath;

public:
  TernaryOp(Type &type, std::string &&name, Value &a, Value &b, Value &c, Op op,
            FastMathFlags fmath = {});

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
            FPExt, FPTrunc, Ptr2Int, Int2Ptr };

private:
  Value *val;
  Op op;

public:
  ConversionOp(Type &type, std::string &&name, Value &val, Op op)
    : Instr(type, std::move(name)), val(&val), op(op) {}

  Op getOp() const { return op; }
  Value& getValue() const { return *val; }
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

  Value *getTrueValue() const { return a; }
  Value *getFalseValue() const { return b; }

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


class InsertValue final : public Instr {
  Value *val, *elt;
  std::vector<unsigned> idxs;
public:
  InsertValue(Type &type, std::string &&name, Value &val, Value &elt)
          : Instr(type, std::move(name)), val(&val), elt(&elt) {}
  void addIdx(unsigned idx);

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class FnCall final : public Instr {
private:
  std::string fnName;
  std::vector<std::pair<Value*, ParamAttrs>> args;
  FnAttrs attrs;
  bool valid;
public:
  FnCall(Type &type, std::string &&name, std::string &&fnName,
         FnAttrs &&attrs = FnAttrs::None, bool valid = true)
    : Instr(type, std::move(name)), fnName(std::move(fnName)),
      attrs(std::move(attrs)), valid(valid) {}
  void addArg(Value &arg, ParamAttrs &&attrs);
  const auto& getFnName() const { return fnName; }
  const auto& getArgs() const { return args; }
  const auto& getAttributes() const { return attrs; }

  bool hasAttribute(const FnAttrs::Attribute &i) const { return attrs.has(i); }
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

private:
  Value *a, *b;
  Cond cond;
  FastMathFlags fmath;

public:
  FCmp(Type &type, std::string &&name, Cond cond, Value &a, Value &b,
       FastMathFlags fmath)
    : Instr(type, move(name)), a(&a), b(&b), cond(cond), fmath(fmath) {}

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
    const unsigned getIdx() const { return idx; }

  };

  class it_helper {
    JumpInstr *instr;
  public:
    it_helper(JumpInstr *instr) : instr(instr) {}
    target_iterator begin() const { return { instr, 0 }; }
    target_iterator end() const;
  };
  it_helper targets() { return this; }
  unsigned getTargetCount() { return it_helper(this).end().getIdx(); }
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


class MemInstr : public Instr {
public:
  MemInstr(Type &type, std::string &&name) : Instr(type, move(name)) {}

  // If this instruction allocates a memory block, return its size.
  // Return 0 if it doesn't allocate anything.
  virtual uint64_t getMaxAllocSize() const = 0;

  // If this instruction performs load or store, return its max access size.
  virtual uint64_t getMaxAccessSize() const = 0;

  // If this instruction performs pointer arithmetic, return the absolute
  // value of the adding offset.
  // If this instruction is accessing the memory, it is okay to return 0.
  // ex) Given `store i32 0, ptr`, 0 can be returned, because its access size
  // already contains the offset.
  virtual uint64_t getMaxGEPOffset() const = 0;

  struct ByteAccessInfo {
    // Does this instruction use integer (pointer) value of a byte?
    // If it stores poison value (e.g. uninitialized bytes of alloca), it is
    // okay for both variables to be false.
    bool hasIntByteAccess = false;
    bool hasPtrByteAccess = false;
    // Does this intruction load / store pointers?
    // If hasPtrByteAccess is false, these cannot be true.
    bool doesPtrLoad = false;
    bool doesPtrStore = false;
    // The maximum size of a byte that this instruction can support.
    // If zero, this instruction does not read/write bytes.
    // Otherwise, bytes of a memory can be widened to this size.
    unsigned byteSize = 0;
    // Does this instruction have sub-byte access (less than 8 bits)?
    bool hasSubByteAccess = false;

    bool doesMemAccess() const { return byteSize; }

    static ByteAccessInfo intOnly(unsigned byteSize);
    static ByteAccessInfo get(const Type &t, bool store, unsigned align);
    static ByteAccessInfo full(unsigned byteSize, bool subByte = false);
  };

  virtual ByteAccessInfo getByteAccessInfo() const = 0;
};

 
class Alloc final : public MemInstr {
  Value *size, *mul;
  unsigned align;
  bool initially_dead;
public:
  Alloc(Type &type, std::string &&name, Value &size, Value *mul, unsigned align,
        bool initially_dead)
    : MemInstr(type, std::move(name)), size(&size), mul(mul), align(align),
      initially_dead(initially_dead) {}

  Value& getSize() const { return *size; }
  Value* getMul() const { return mul; }
  bool initDead() const { return initially_dead; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Malloc final : public MemInstr {
  Value *ptr = nullptr, *size;
  // Is this malloc (or equivalent operation, like new()) never returning
  // null?
  bool isNonNull = false;

public:
  Malloc(Type &type, std::string &&name, Value &size, bool isNonNull)
    : MemInstr(type, std::move(name)), size(&size), isNonNull(isNonNull) {}

  Malloc(Type &type, std::string &&name, Value &ptr, Value &size)
    : MemInstr(type, std::move(name)), ptr(&ptr), size(&size) {}

  Value& getSize() const { return *size; }
  bool isRealloc() const { return ptr != nullptr; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Calloc final : public MemInstr {
  Value *num, *size;
public:
  Calloc(Type &type, std::string &&name, Value &num, Value &size)
    : MemInstr(type, std::move(name)), num(&num), size(&size) {}

  Value& getNum() const { return *num; }
  Value& getSize() const { return *size; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class StartLifetime final : public MemInstr {
  Value *ptr;
public:
  StartLifetime(Value &ptr) : MemInstr(Type::voidTy, "start_lifetime"),
      ptr(&ptr) {}

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Free final : public MemInstr {
  Value *ptr;
  bool heaponly;
public:
  Free(Value &ptr, bool heaponly = true) : MemInstr(Type::voidTy, "free"),
      ptr(&ptr), heaponly(heaponly) {}

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class GEP final : public MemInstr {
  Value *ptr;
  std::vector<std::pair<unsigned, Value*>> idxs;
  bool inbounds;
public:
  GEP(Type &type, std::string &&name, Value &ptr, bool inbounds)
    : MemInstr(type, std::move(name)), ptr(&ptr), inbounds(inbounds) {}

  void addIdx(unsigned obj_size, Value &idx);
  Value& getPtr() const { return *ptr; }
  auto& getIdxs() const { return idxs; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Load final : public MemInstr {
  Value *ptr;
  unsigned align;
public:
  Load(Type &type, std::string &&name, Value &ptr, unsigned align)
    : MemInstr(type, std::move(name)), ptr(&ptr), align(align) {}

  Value& getPtr() const { return *ptr; }
  unsigned getAlign() const { return align; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Store final : public MemInstr {
  Value *ptr, *val;
  unsigned align;
public:
  Store(Value &ptr, Value &val, unsigned align)
    : MemInstr(Type::voidTy, "store"), ptr(&ptr), val(&val), align(align) {}

  Value& getValue() const { return *val; }
  Value& getPtr() const { return *ptr; }
  unsigned getAlign() const { return align; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxAccessStride() const;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Memset final : public MemInstr {
  Value *ptr, *val, *bytes;
  unsigned align;
public:
  Memset(Value &ptr, Value &val, Value &bytes, unsigned align)
    : MemInstr(Type::voidTy, "memset"), ptr(&ptr), val(&val), bytes(&bytes),
            align(align) {}

  Value& getBytes() const { return *bytes; }
  unsigned getAlign() const { return align; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Memcpy final : public MemInstr {
  Value *dst, *src, *bytes;
  unsigned align_dst, align_src;
  bool move;
public:
  Memcpy(Value &dst, Value &src, Value &bytes,
         unsigned align_dst, unsigned align_src, bool move)
    : MemInstr(Type::voidTy, "memcpy"), dst(&dst), src(&src), bytes(&bytes),
            align_dst(align_dst), align_src(align_src), move(move) {}

  Value& getBytes() const { return *bytes; }
  unsigned getSrcAlign() const { return align_src; }
  unsigned getDstAlign() const { return align_dst; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Memcmp final : public MemInstr {
  Value *ptr1, *ptr2, *num;
  bool is_bcmp;
public:
  Memcmp(Type &type, std::string &&name, Value &ptr1, Value &ptr2, Value &num,
         bool is_bcmp): MemInstr(type, std::move(name)), ptr1(&ptr1),
                        ptr2(&ptr2), num(&num), is_bcmp(is_bcmp) {}

  Value &getBytes() const { return *num; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


class Strlen final : public MemInstr {
  Value *ptr;
public:
  Strlen(Type &type, std::string &&name, Value &ptr)
    : MemInstr(type, std::move(name)), ptr(&ptr) {}

  Value *getPointer() const { return ptr; }

  uint64_t getMaxAllocSize() const override;
  uint64_t getMaxAccessSize() const override;
  uint64_t getMaxGEPOffset() const override;
  ByteAccessInfo getByteAccessInfo() const override;

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
  Value *v1, *v2;
  std::vector<unsigned> mask;
public:
  ShuffleVector(Type &type, std::string &&name, Value &v1, Value &v2,
                std::vector<unsigned> mask)
    : Instr(type, move(name)), v1(&v1), v2(&v2), mask(std::move(mask)) {}
  std::vector<Value*> operands() const override;
  void rauw(const Value &what, Value &with) override;
  void print(std::ostream &os) const override;
  StateValue toSMT(State &s) const override;
  smt::expr getTypeConstraints(const Function &f) const override;
  std::unique_ptr<Instr> dup(const std::string &suffix) const override;
};


const ConversionOp *isCast(ConversionOp::Op op, const Value &v);
Value *isNoOp(const Value &v);
}
