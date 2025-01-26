// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "cache/cache.h"
#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;
using namespace llvm;

#define LLVM_ARGS_PREFIX ""
#define ARGS_SRC_TGT
#define ARGS_REFINEMENT
#include "llvm_util/cmd_args_list.h"

namespace {

cl::opt<long>
    opt_rand_seed(LLVM_ARGS_PREFIX "seed",
                  cl::desc("Random seed (default=different every time)"),
                  cl::cat(alive_cmdargs), cl::init(0));

cl::opt<long> opt_num_reps(
    LLVM_ARGS_PREFIX "num-reps",
    cl::desc("Number of times to generate and test a function (default=10)"),
    cl::cat(alive_cmdargs), cl::init(10));

cl::opt<bool>
    opt_skip_alive(LLVM_ARGS_PREFIX "skip-alive",
                   cl::desc("Generate IR but then don't invoke Alive"),
                   cl::cat(alive_cmdargs), cl::init(false));

cl::opt<bool> opt_run_sroa(
    LLVM_ARGS_PREFIX "run-sroa",
    cl::desc("Run SROA before llvm2alive, this reduces timeouts by reducing "
             "the load on Alive's memory model"),
    cl::cat(alive_cmdargs), cl::init(false));

cl::opt<bool> opt_print_ir(LLVM_ARGS_PREFIX "print-ir",
                           cl::desc("Print LLVM IR to stdout"),
                           cl::cat(alive_cmdargs), cl::init(false));

cl::opt<bool> opt_run_dce(
    LLVM_ARGS_PREFIX "run-dce",
    cl::desc(
        "Run dead code elimination before llvm2alive, cleaning up generated "
        "code and making it easier to read"),
    cl::cat(alive_cmdargs), cl::init(false));

cl::opt<string>
    opt_fuzzer(LLVM_ARGS_PREFIX "fuzzer",
               cl::desc("Which fuzzer to run; choices are value and bb. "
                        "See the --help output for more information. "
                        "(default=value)"),
               cl::cat(alive_cmdargs), cl::init("value"));

cl::opt<string>
    optPass(LLVM_ARGS_PREFIX "passes", cl::value_desc("optimization passes"),
            cl::desc("Specify which LLVM passes to run (default=O2). "
                     "The syntax is described at "
                     "https://llvm.org/docs/NewPassManager.html#invoking-opt"),
            cl::cat(alive_cmdargs), cl::init("O2"));

class Chooser {
  mt19937_64 Rand;
  Chooser() = delete;

public:
  Chooser(long seed) : Rand(seed) {}

  long choose(long Choices) {
    return uniform_int_distribution<int>(0, Choices - 1)(Rand);
  }

  bool flip() {
    return choose(2) == 0;
  }

  long dist() {
    uniform_int_distribution<unsigned long> Dist(
        0, numeric_limits<unsigned long>::max());
    return Dist(Rand);
  }
};

class ValueGenerator {
  enum class WidthPolicy {
    Wild = 0,
    Chosen,
    AllOne,
    Mixed1,
    Mixed2,
    Mixed3,
    None
  };
  const array<int, 5> ChosenIntWidths{1, 8, 16, 32, 64};
  int SavedWidth = -1;
  WidthPolicy WP = WidthPolicy::None;
  BasicBlock *BB{nullptr};
  vector<vector<APInt>> Pool;
  vector<Value *> Vals;
  Chooser &C;
  const int MaxIntWidth;
  unordered_map<Value *, Type *> tyMap;
  LLVMContext &Ctx;

public:
  void addVal(Value *v) {
    Vals.push_back(v);
  }

  void setBB(BasicBlock *_BB) {
    BB = _BB;
  }

  ValueGenerator(Chooser &_C, int _MaxIntWidth, LLVMContext &_Ctx)
      : C(_C), MaxIntWidth(_MaxIntWidth), Ctx(_Ctx) {
    WP = (WidthPolicy)C.choose(6);
    SavedWidth =
        C.flip() ? getWidth(WidthPolicy::Wild) : getWidth(WidthPolicy::Chosen);
    for (int i = 0; i <= MaxIntWidth; ++i)
      Pool.push_back(vector<APInt>());
  }

  int getWidth() {
    return getWidth(WP);
  }

  void addArgTy(Argument &arg, Type *ty) {
    tyMap[&arg] = ty;
  }

  Type *getArgTy(Value *arg) {
    return tyMap[arg];
  }

  Value *adapt(Value *Val, Type *Ty, const Twine &Name = Twine()) {
    if (C.choose(15) == 0)
      Val = new FreezeInst(Val, "", BB);
    auto Width = Val->getType()->getIntegerBitWidth();
    auto DesiredWidth = Ty->getIntegerBitWidth();
    if (DesiredWidth == Width)
      return Val;
    if (DesiredWidth < Width)
      return new TruncInst(Val, Ty, Name, BB);
    if (C.flip())
      return new SExtInst(Val, Ty, Name, BB);
    else
      return new ZExtInst(Val, Ty, Name, BB);
  }

  CmpInst::Predicate randomPred() {
    return (CmpInst::Predicate)(CmpInst::FIRST_ICMP_PREDICATE +
                                C.choose(CmpInst::LAST_ICMP_PREDICATE + 1 -
                                         CmpInst::FIRST_ICMP_PREDICATE));
  }

  Value *getVal(Type *Ty) {
    int num_vals = Vals.size();
    int idx = C.choose(1 + num_vals);
    if (idx == num_vals) {
      return randomInt(Ty);
    } else {
      auto *v = Vals[idx];
      if (v->getType()->isPointerTy())
        v = new LoadInst(getArgTy(v), v, "", false, BB);
      return adapt(v, Ty);
    }
  }

  // TODO generate vector instructions
  Value *genInst() {
    auto *Ty = Type::getIntNTy(BB->getContext(), getWidth(WP));
    Value *Val = nullptr;
    switch (C.choose(4)) {
    case 0: {
      auto *LHS = getVal(Ty);
      auto *RHS = getVal(Ty);
      Val = new ICmpInst(BB, randomPred(), LHS, RHS);
    } break;
    case 1: {
      auto *LHS = getVal(Ty);
      Value *RHS = getVal(Ty);
      auto NarrowWidth = ilog2_ceil(Ty->getIntegerBitWidth() - 1, true);
      auto *NarrowTy = Type::getIntNTy(BB->getContext(), NarrowWidth);
      auto *Mask = ConstantInt::get(Ty, (1UL << NarrowWidth) - 1);
      auto *AltRHS = C.flip() ? adapt(adapt(RHS, NarrowTy, "mask"), Ty, "mask")
                              : BinaryOperator::Create(BinaryOperator::And, RHS,
                                                       Mask, "mask", BB);
      BinaryOperator *BinOp = randomBinop(LHS, PoisonValue::get(Ty));
      Val = BinOp;
      auto Op = BinOp->getOpcode();
      bool isConst = isa<ConstantInt>(RHS);
      // all literal constants and some variable inputs should be
      // truncated to avoid too much poison due to OOB shift exponents
      if ((Op == BinaryOperator::LShr || Op == BinaryOperator::AShr ||
           Op == BinaryOperator::Shl) &&
          (isConst || C.flip())) {
        BinOp->setOperand(1, AltRHS);
      } else {
        BinOp->setOperand(1, RHS);
      }
    } break;
    case 2: {
      auto *LHS = getVal(Ty);
      auto *RHS = getVal(Ty);
      auto *Cond = getVal(Type::getInt1Ty(BB->getContext()));
      Val = SelectInst::Create(Cond, LHS, RHS, "", BB);
    } break;
    case 3: {
      switch (C.choose(3)) {
      case 0:
        Val = genUnaryIntrinsic(Ty);
        break;
      case 1:
        Val = genBinaryIntrinsic(Ty);
        break;
      case 2:
        Val = genTernaryIntrinsic(Ty);
        break;
      default:
        assert(false);
      }
    } break;
    default:
      assert(false);
    }
    Vals.push_back(Val);
    return Val;
  }

private:
  BinaryOperator *randomBinop(Value *LHS, Value *RHS) {
    switch (C.choose(13)) {
    case 0: {
      auto *I = BinaryOperator::Create(BinaryOperator::Add, LHS, RHS, "", BB);
      if (C.flip())
        I->setHasNoSignedWrap();
      if (C.flip())
        I->setHasNoUnsignedWrap();
      return I;
    }
    case 1: {
      auto *I = BinaryOperator::Create(BinaryOperator::Sub, LHS, RHS, "", BB);
      if (C.flip())
        I->setHasNoSignedWrap();
      if (C.flip())
        I->setHasNoUnsignedWrap();
      return I;
    }
    case 2: {
      auto *I = BinaryOperator::Create(BinaryOperator::Mul, LHS, RHS, "", BB);
      if (C.flip())
        I->setHasNoSignedWrap();
      if (C.flip())
        I->setHasNoUnsignedWrap();
      return I;
    }
    case 3: {
      auto *I = BinaryOperator::Create(BinaryOperator::UDiv, LHS, RHS, "", BB);
      if (C.flip())
        I->setIsExact();
      return I;
    }
    case 4: {
      auto *I = BinaryOperator::Create(BinaryOperator::SDiv, LHS, RHS, "", BB);
      if (C.flip())
        I->setIsExact();
      return I;
    }
    case 5:
      return BinaryOperator::Create(BinaryOperator::URem, LHS, RHS, "", BB);
    case 6:
      return BinaryOperator::Create(BinaryOperator::SRem, LHS, RHS, "", BB);
    case 7: {
      auto *I = BinaryOperator::Create(BinaryOperator::Shl, LHS, RHS, "", BB);
      if (C.flip())
        I->setHasNoSignedWrap();
      if (C.flip())
        I->setHasNoUnsignedWrap();
      return I;
    }
    case 8: {
      auto *I = BinaryOperator::Create(BinaryOperator::LShr, LHS, RHS, "", BB);
      if (C.flip())
        I->setIsExact();
      return I;
    }
    case 9: {
      auto *I = BinaryOperator::Create(BinaryOperator::AShr, LHS, RHS, "", BB);
      if (C.flip())
        I->setIsExact();
      return I;
    }
    case 10:
      return BinaryOperator::Create(BinaryOperator::And, LHS, RHS, "", BB);
    case 11:
      return BinaryOperator::Create(BinaryOperator::Or, LHS, RHS, "", BB);
    case 12:
      return BinaryOperator::Create(BinaryOperator::Xor, LHS, RHS, "", BB);
    default:
      assert(false);
    }
  }

  // uniformly chosen 16-bit constant -- we'll get them all eventually
  APInt uniform16(int Width) {
    return APInt(Width, C.choose(0xFFFF + 1));
  }

  // uniformly choose a Hamming weight and then uniformly select a value
  // having that weight (but only the bottom half of weights, we'll
  // randomly flip the bits later)
  APInt hamming(int Width) {
    auto Weight = 1 + C.choose((Width / 2) - 1);
    APInt I(Width, 0);
    int Remaining = Weight;
    while (Remaining > 0) {
      auto Bit = C.choose(Width);
      if (I[Bit])
        continue;
      I.setBit(Bit);
      --Remaining;
    }
    return I;
  }

  // one or more runs of set bits
  APInt bitRun(int Width) {
    APInt I(Width, 0);
    do {
      auto loBit = C.choose(Width - 1);
      auto hiBit = loBit + 1 + C.choose(Width / 4);
      if (hiBit > Width)
        hiBit = Width;
      I.setBits(loBit, hiBit);
    } while (C.flip());
    return I;
  }

  // numbers of the form ±2^i ± 2^j ± 2^k
  APInt sumOfPowers(int Width) {
    APInt I = APInt(Width, 0);
    do {
      APInt X(Width, 0);
      X.setBit(C.choose(Width));
      I += C.flip() ? X : -X;
    } while (C.flip());
    return I;
  }

  // uniform random choice from the entire range
  APInt uniformInt(int Width) {
    return APInt(Width, C.dist());
  }

  // chosen values
  APInt selectedInt(int Width) {
    switch (C.choose(5)) {
    case 0:
      return APInt::getZero(Width);
    case 1:
      return APInt(Width, 1);
    case 2:
      return APInt::getMaxValue(Width);
    case 3:
      return APInt::getSignedMaxValue(Width);
    case 4:
      return APInt::getSignedMinValue(Width);
    default:
      assert(false);
    }
  }

  APInt randomIntHelper(int Width) {
    if (Width == 1)
      return uniformInt(Width);

    if (Width <= 8)
      return C.flip() ? uniformInt(Width) : selectedInt(Width);

    switch (C.choose(6)) {
    case 0:
      return uniform16(Width);
    case 1:
      return hamming(Width);
    case 2:
      return bitRun(Width);
    case 3:
      return sumOfPowers(Width);
    case 4:
      return uniformInt(Width);
    case 5:
      return selectedInt(Width);
    default:
      assert(false);
    }
  }

  Constant *randomInt(Type *Ty) {
    const auto Width = Ty->getIntegerBitWidth();
    auto &P = Pool.at(Width);

    if (P.size() > 0 && C.flip()) {
      auto I = P.at(C.choose(P.size()));
      switch (C.choose(3)) {
      case 0: {
        APInt Delta(Width, C.choose(8) + 1);
        I += C.flip() ? Delta : -Delta;
        break;
      }
      case 1:
        I ^= APInt(Width, 1 << C.choose(Width));
        break;
      case 2:
        I = ~I;
        break;
      default:
        assert(false);
      }
      return ConstantInt::get(Ty, I);
    } else {
      auto I = randomIntHelper(Width);
      switch (C.choose(5)) {
      case 0:
        I = ~I;
        break;
      case 1:
        I = I + 1;
        break;
      case 2:
        I = I - 1;
        break;
      default:
        break;
      }
      P.push_back(I);
      return ConstantInt::get(Ty, I);
    }
  }

  Value *getRandomVal(unsigned Upto, Type *Ty) {
    const auto Width = Ty->getIntegerBitWidth();
    auto &P = Pool.at(Width);
    APInt I(Width, C.choose(Upto));
    P.push_back(I);
    return ConstantInt::get(Ty, I);
  }

  int getWidth(WidthPolicy P) {
    switch (P) {
    case WidthPolicy::Wild:
      return 1 + C.choose(MaxIntWidth);
    case WidthPolicy::Chosen: {
      int W;
      do {
        W = ChosenIntWidths[C.choose(ChosenIntWidths.size())];
      } while (W > MaxIntWidth);
      return W;
    }
    case WidthPolicy::AllOne:
      return SavedWidth;
    case WidthPolicy::Mixed1:
      return getWidth(C.flip() ? WidthPolicy::Chosen : WidthPolicy::Wild);
    case WidthPolicy::Mixed2:
      if (C.flip())
        return SavedWidth;
      else
        return getWidth(WidthPolicy::Chosen);
    case WidthPolicy::Mixed3:
      if (C.flip())
        return getWidth(WidthPolicy::Wild);
      else
        return SavedWidth;
    default:
      assert(false);
    }
  }

  Value *genUnaryIntrinsic(Type *Ty) {
    auto *M = BB->getModule();
    auto *Arg = getVal(Ty);

    Intrinsic::ID Op;
  again:
    switch (C.choose(6)) {
    case 0:
      Op = Intrinsic::abs;
      break;
    case 1:
      Op = Intrinsic::bitreverse;
      break;
    case 2:
      if ((Ty->getIntegerBitWidth() % 16) != 0)
        goto again;
      Op = Intrinsic::bswap;
      break;
    case 3:
      Op = Intrinsic::ctpop;
      break;
    case 4:
      Op = Intrinsic::ctlz;
      break;
    case 5:
      Op = Intrinsic::cttz;
      break;
    default:
      assert(false);
    }

    vector<Value *> Args = {Arg};
    if (Op == Intrinsic::abs || Op == Intrinsic::ctlz || Op == Intrinsic::cttz)
      Args.push_back(ConstantInt::get(Type::getInt1Ty(Ctx), C.flip() ? 1 : 0));

    auto Decl = Intrinsic::getOrInsertDeclaration(M, Op, Ty);
    auto *I = CallInst::Create(Decl, Args, "", BB);
    return I;
  }

  Value *genBinaryIntrinsic(Type *Ty) {
    auto *M = BB->getModule();
    auto *LHS = getVal(Ty);
    auto *RHS = getVal(Ty);

    Intrinsic::ID Op;
    switch (C.choose(16)) {
    case 0:
      Op = Intrinsic::ssub_with_overflow;
      break;
    case 1:
      Op = Intrinsic::usub_with_overflow;
      break;
    case 2:
      Op = Intrinsic::sadd_with_overflow;
      break;
    case 3:
      Op = Intrinsic::uadd_with_overflow;
      break;
    case 4:
      Op = Intrinsic::smul_with_overflow;
      break;
    case 5:
      Op = Intrinsic::umul_with_overflow;
      break;
    case 6:
      Op = Intrinsic::uadd_sat;
      break;
    case 7:
      Op = Intrinsic::sadd_sat;
      break;
    case 8:
      Op = Intrinsic::usub_sat;
      break;
    case 9:
      Op = Intrinsic::ssub_sat;
      break;
    case 10:
      Op = Intrinsic::smax;
      break;
    case 11:
      Op = Intrinsic::smin;
      break;
    case 12:
      Op = Intrinsic::umax;
      break;
    case 13:
      Op = Intrinsic::umin;
      break;
    case 14:
      Op = Intrinsic::sshl_sat;
      break;
    case 15:
      Op = Intrinsic::ushl_sat;
      break;
    default:
      assert(false);
    }

    auto Decl = Intrinsic::getOrInsertDeclaration(M, Op, Ty);
    auto *I = CallInst::Create(Decl, {LHS, RHS}, "", BB);
    if (Op == Intrinsic::ssub_with_overflow ||
        Op == Intrinsic::usub_with_overflow ||
        Op == Intrinsic::sadd_with_overflow ||
        Op == Intrinsic::uadd_with_overflow ||
        Op == Intrinsic::smul_with_overflow ||
        Op == Intrinsic::umul_with_overflow) {
      auto *Ov = ExtractValueInst::Create(I, {1}, "", BB);
      Vals.push_back(Ov);
      return ExtractValueInst::Create(I, {0}, "", BB);
    } else {
      return I;
    }
  }

  Value *genTernaryIntrinsic(Type *Ty) {
    auto *M = BB->getModule();
    auto *A = getVal(Ty);
    auto *B = getVal(Ty);
    bool force_32bits_imm = false;
    Intrinsic::ID Op;

    switch (C.choose(6)) {
    case 0:
      Op = Intrinsic::fshl;
      break;
    case 1:
      Op = Intrinsic::fshr;
      break;
    case 2:
      Op = Intrinsic::smul_fix;
      force_32bits_imm = true;
      break;
    case 3:
      Op = Intrinsic::umul_fix;
      force_32bits_imm = true;
      break;
    case 4:
      Op = Intrinsic::smul_fix_sat;
      force_32bits_imm = true;
      break;
    case 5:
      Op = Intrinsic::umul_fix_sat;
      force_32bits_imm = true;
      break;
    default:
      UNREACHABLE();
    }
    auto *C = force_32bits_imm
                  ? getRandomVal(Ty->getIntegerBitWidth(),
                                 Type::getInt32Ty(BB->getContext()))
                  : getVal(Ty);

    auto Decl = Intrinsic::getOrInsertDeclaration(M, Op, Ty);
    auto *I = CallInst::Create(Decl, {A, B, C}, "", BB);
    return I;
  }
};

class Fuzzer {
public:
  virtual ~Fuzzer() {}
  virtual void go() = 0;
};

class ValueFuzzer : public Fuzzer {
  const int MaxIntWidth = 64;
  const int MaxIntParams = 5;
  const int MaxInsts = 15;
  Module &M;
  LLVMContext &Ctx;
  Chooser C;
  ValueGenerator VG;
  bool gone = false;

public:
  ValueFuzzer(Module &_M, long seed) : M(_M), Ctx(M.getContext()), C(seed),
    VG(C, MaxIntWidth, Ctx) {}

  void go() override;
};

void ValueFuzzer::go() {
  assert(!gone);
  gone = true;

  const int NumIntParams = 1 + C.choose(MaxIntParams);
  auto *RetTy = Type::getIntNTy(Ctx, VG.getWidth());
  vector<Type *> ParamsTy, ParamsRealTy;
  for (int i = 0; i < NumIntParams; ++i) {
    auto *origTy = Type::getIntNTy(Ctx, VG.getWidth());
    auto *realTy =
        C.flip() ? (Type *)origTy : (Type *)PointerType::get(Ctx, 0);
    ParamsRealTy.push_back(origTy);
    ParamsTy.push_back(realTy);
  }
  auto *FTy = FunctionType::get(RetTy, ParamsTy, false);
  auto *F = Function::Create(FTy, GlobalValue::ExternalLinkage, 0, "f", &M);
  if (C.flip()) {
    F->addRetAttr(C.flip() ? Attribute::ZExt : Attribute::SExt);
  }
  if (C.choose(4) == 0)
    F->addRetAttr(Attribute::NoUndef);
  auto BB = BasicBlock::Create(Ctx, "", F);
  VG.setBB(BB);

  vector<Value *> PointerParams;
  int idx = 0;
  for (auto &arg : F->args()) {
    VG.addVal(&arg);
    if (arg.getType()->isPointerTy()) {
      PointerParams.push_back(&arg);
      VG.addArgTy(arg, ParamsRealTy[idx]);
    } else {
      if (C.flip()) {
        arg.addAttr(C.flip() ? Attribute::ZExt : Attribute::SExt);
      }
    }
    if (C.choose(4) == 0)
      arg.addAttr(Attribute::NoUndef);
    ++idx;
  }

  int num_insts = C.choose(MaxInsts);
  for (int i = 0; i < num_insts; ++i) {
    auto *v = VG.genInst();
    if (C.choose(4) == 0 && !PointerParams.empty()) {
      auto *p = PointerParams[C.choose(PointerParams.size())];
      new StoreInst(VG.adapt(v, VG.getArgTy(p)), p, BB);
    }
  }

  ReturnInst::Create(Ctx, VG.getVal(RetTy), BB);
}

class BBFuzzer : public Fuzzer {
  const int MaxBBs = 50;
  const int MaxWidth = 20;
  const int MaxCounters = 16;
  const int MaxBoolParams = 16;
  Module &M;
  LLVMContext &Ctx;
  Chooser C;
  ValueGenerator VG;
  bool gone = false;

public:
  BBFuzzer(Module &_M, long seed) : M(_M), Ctx(M.getContext()), C(seed),
    VG(C, MaxWidth, Ctx) {}

  void go() override;
};

void BBFuzzer::go() {
  assert(!gone);
  gone = true;

  const int NumCounters = 1 + C.choose(MaxCounters);
  const int NumBoolParams = 1 + C.choose(MaxBoolParams);
  const int NumBBs = 2 + C.choose(MaxBBs);
  const int Width = 1 + C.choose(MaxWidth);
  auto *IntTy = Type::getIntNTy(Ctx, Width);

  auto *Callee1Ty = FunctionType::get(Type::getVoidTy(Ctx), {}, false);
  FunctionCallee Callee1 = M.getOrInsertFunction("c1", Callee1Ty);

  auto *Callee2Ty = FunctionType::get(Type::getVoidTy(Ctx), {IntTy}, false);
  FunctionCallee Callee2 = M.getOrInsertFunction("c2", Callee2Ty);

  vector<Type *> ParamsTy;
  auto *BoolTy = Type::getInt1Ty(Ctx);
  for (int i = 0; i < NumBoolParams; ++i)
    ParamsTy.push_back(BoolTy);
  auto *FTy = FunctionType::get(IntTy, ParamsTy, false);
  auto *F = Function::Create(FTy, GlobalValue::ExternalLinkage, 0, "f", &M);
  vector<Argument *> Args;
  for (auto &arg : F->args())
    Args.push_back(&arg);
  int FirstBoolParamIdx = 0;

  vector<BasicBlock *> BBs;
  for (int i = 0; i < NumBBs; ++i)
    BBs.push_back(BasicBlock::Create(Ctx, "", F));

  vector<Value *> Counters;
  auto *Zero = ConstantInt::get(IntTy, 0);
  auto *One = ConstantInt::get(IntTy, 1);
  for (int i = 0; i < NumCounters; ++i) {
    Counters.push_back(new AllocaInst(IntTy, 0, One, "", BBs[0]));
    new StoreInst(Zero, Counters[i], BBs[0]);
  }

  for (int i = 0; i < NumBBs; ++i) {
    if (C.choose(10) == 0) {
      if (C.flip()) {
        auto idx = C.choose(NumCounters);
        auto *Load = new LoadInst(IntTy, Counters[idx], "", BBs[i]);
        CallInst::Create(Callee2, {Load}, "", BBs[i]);
      } else {
        CallInst::Create(Callee1, {}, "", BBs[i]);
      }
    } else {
      auto idx = C.choose(NumCounters);
      auto *Load = new LoadInst(IntTy, Counters[idx], "", BBs[i]);
      auto *Inc =
          BinaryOperator::Create(Instruction::Add, Load, One, "", BBs[i]);
      if (C.flip())
        Inc->setHasNoUnsignedWrap();
      if (C.flip())
        Inc->setHasNoSignedWrap();
      new StoreInst(Inc, Counters[idx], BBs[i]);
    }
  }

  for (int i = 0; i < NumBBs; ++i) {
  again:
    switch (C.choose(4)) {
    case 0: {
      if (i == 0 || (C.choose(100) > 25))
        goto again;
      auto idx = C.choose(NumCounters);
      auto *Load = new LoadInst(IntTy, Counters[idx], "", BBs[i]);
      ReturnInst::Create(Ctx, Load, BBs[i]);
    } break;
    case 1: {
      auto *Dest = BBs[1 + C.choose(NumBBs - 1)];
      BranchInst::Create(Dest, BBs[i]);
    } break;
    case 2: {
      auto *Dest1 = BBs[1 + C.choose(NumBBs - 1)];
      auto *Dest2 = BBs[1 + C.choose(NumBBs - 1)];
      Value *Cond = nullptr;
      if (C.flip()) {
        Cond = Args[FirstBoolParamIdx + C.choose(NumBoolParams)];
      } else {
        auto *LHS =
            new LoadInst(IntTy, Counters[C.choose(NumCounters)], "", BBs[i]);
        auto *RHS = (C.flip())
                        ? new LoadInst(IntTy, Counters[C.choose(NumCounters)],
                                       "", BBs[i])
                        : (Value *)ConstantInt::get(IntTy, C.choose(20));
        Cond = new ICmpInst(BBs[i], VG.randomPred(), LHS, RHS);
      }
      BranchInst::Create(Dest1, Dest2, Cond, BBs[i]);
    } break;
    case 3: {
      unsigned long NumCases = 1 + C.choose(2 * NumBBs);
      auto *Load =
          new LoadInst(IntTy, Counters[C.choose(NumCounters)], "", BBs[i]);
      auto *Swch = SwitchInst::Create(Load, BBs[1 + C.choose(NumBBs - 1)],
                                      NumCases, BBs[i]);
      for (unsigned long i = 0; i < NumCases; ++i) {
        if (i >= (1UL << Width))
          break;
        Swch->addCase(ConstantInt::get(IntTy, i),
                      BBs[1 + C.choose(NumBBs - 1)]);
        if (C.choose(4) == 0)
          i += C.choose(4);
      }
    } break;
    default:
      assert(false);
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::InitLLVM X(argc, argv);
  EnableDebugBuffering = true;
  LLVMContext Context;

  string Usage =
      R"EOF(Alive2 simple generative fuzzer:
version )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see quick-fuzz --version for LLVM version info,

This program stress-tests LLVM and Alive2 by performing randomized
generation of LLVM functions, optimizing them, and then checking
refinement.

It currently contains two simple generators: "value," which generates
a single basic block containing integer operations, and "bb," which
exercises loop and control flow optimizations.

The recommended workflow is to run quick-fuzz until it finds an issue,
and then re-run with the same seed and also the --save-ir command line
option, in order to get a standalone test case that can then be
reduced using llvm-reduce.
)EOF";

  cl::HideUnrelatedOptions(alive_cmdargs);
  cl::ParseCommandLineOptions(argc, argv, Usage);

  unique_ptr<Cache> cache;
  unique_ptr<Module> MDummy;
#define ARGS_MODULE_VAR MDummy
#include "llvm_util/cmd_args_def.h"

  Module M1("fuzz", Context);
  auto &DL = M1.getDataLayout();
  Triple targetTriple(M1.getTargetTriple());
  TargetLibraryInfoWrapperPass TLI(targetTriple);

  llvm_util::initializer llvm_util_init(*out, DL);
  smt::smt_initializer smt_init;
  Verifier verifier(TLI, smt_init, *out);
  verifier.always_verify = opt_always_verify;
  verifier.print_dot = opt_print_dot;
  verifier.bidirectional = opt_bidirectional;

  function<unique_ptr<Fuzzer>(Module &, long)> makeFuzzer;
  if (opt_fuzzer == "value") {
    makeFuzzer = [](Module &M, long seed) {
      return make_unique<ValueFuzzer>(M, seed);
    };
  } else if (opt_fuzzer == "bb") {
    makeFuzzer = [](Module &M, long seed) {
      return make_unique<BBFuzzer>(M, seed);
    };
  } else {
    *out << "Available fuzzers are \"value\" and \"bb\".\n\n";
    exit(-1);
  }

  mt19937_64 Rand((opt_rand_seed == 0) ? random_device{}() : opt_rand_seed);
  uniform_int_distribution<unsigned long> Dist(
      0, numeric_limits<unsigned long>::max());

  for (int rep = 0; rep < opt_num_reps; ++rep) {
    auto F = makeFuzzer(M1, Dist(Rand));
    F->go();

    if (verifyModule(M1, &errs()))
      report_fatal_error("Broken module found, this should not happen");

    if (opt_run_sroa) {
      auto err = optimize_module(&M1, "sroa,dse");
      assert(err.empty());
    }

    if (opt_run_dce) {
      auto err = optimize_module(&M1, "adce");
      assert(err.empty());
    }

    if (opt_save_ir) {
      stringstream output_fn;
      output_fn << "file_" << rep << ".bc";
      *out << "saving IR as '" << output_fn.str() << "'\n";
      error_code EC;
      raw_fd_ostream output_file(output_fn.str(), EC);
      if (EC)
        report_fatal_error("Couldn't open output file, exiting");
      WriteBitcodeToFile(M1, output_file);
    }

    if (opt_print_ir) {
      out->flush();
      outs() << "------------------------------------------------------\n\n";
      M1.print(outs(), nullptr);
      outs() << "------------------------------------------------------\n\n";
      outs().flush();
    }

    if (opt_skip_alive)
      continue;

    auto M2 = CloneModule(M1);
    auto err = optimize_module(M2.get(), optPass);
    if (!err.empty()) {
      *out << "Error parsing list of LLVM passes: " << err << '\n';
      return -1;
    }

    auto *F1 = M1.getFunction("f");
    auto *F2 = M2->getFunction("f");
    assert(F1 && F2);

    // this is a hack but a useful one. attribute inference sets these
    // and then we always fail Alive's syntactic equality check. so we
    // just go ahead and (soundly) drop them by hand.
    F2->removeFnAttr(Attribute::NoFree);
    F2->removeFnAttr(Attribute::Memory);
    F2->removeFnAttr(Attribute::WillReturn);

    if (!verifier.compareFunctions(*F1, *F2))
      if (opt_error_fatal)
        goto end;

    F1->eraseFromParent();
    vector<Function *> Funcs;
    for (auto &F : M1)
      Funcs.push_back(&F);
    for (auto F : Funcs)
      F->eraseFromParent();
  }

  *out << "Summary:\n"
          "  "
       << verifier.num_correct
       << " correct transformations\n"
          "  "
       << verifier.num_unsound
       << " incorrect transformations\n"
          "  "
       << verifier.num_failed
       << " failed-to-prove transformations\n"
          "  "
       << verifier.num_errors << " Alive2 errors\n";

end:
  if (opt_smt_stats)
    smt::solver_print_stats(*out);

  return verifier.num_errors > 0;
}
