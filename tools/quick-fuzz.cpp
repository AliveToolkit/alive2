// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "backend_tv/lifter.h"
#include "cache/cache.h"
#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
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
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>
#include <array>
#include <fstream>
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

cl::opt<bool>
    opt_backend_tv(LLVM_ARGS_PREFIX "backend-tv",
		   cl::desc("Instead of testing the middle end, test the AArch64 backend"),
                   cl::cat(alive_cmdargs), cl::init(false));

cl::opt<bool> opt_run_sroa(
    LLVM_ARGS_PREFIX "run-sroa",
    cl::desc("Run SROA before llvm2alive, this reduces timeouts by reducing "
             "the load on Alive's memory model"),
    cl::cat(alive_cmdargs), cl::init(false));

cl::opt<bool> opt_print_ir(
    LLVM_ARGS_PREFIX "print-ir",
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

// FIXME generate vectors

// FIXME we might want to be able to specify these on the command
// line, but these should be pretty good defaults
const int MaxBBs = 5;
const int MaxWidth = 64;
const int MaxCounters = 4;
const int MaxBoolParams = 4;

const int MaxIntWidth = 64;
const int MaxIntParams = 5;
const int MaxInsts = 35;

std::optional<std::mt19937_64> Rand;

void initFuzzer() {
  auto Seed = (opt_rand_seed == 0) ? (random_device{}()) : opt_rand_seed;
  cout << "seed = " << Seed << endl;
  Rand = make_optional<mt19937_64>(Seed);
}

long choose(long Choices) {
  return uniform_int_distribution<int>(0, Choices - 1)(*Rand);
}

bool flip() {
  return choose(2) == 0;
}

BinaryOperator *randomBinop(Value *LHS, Value *RHS, BasicBlock *BB) {
  switch (choose(13)) {
  case 0: {
    auto *I = BinaryOperator::Create(BinaryOperator::Add, LHS, RHS, "", BB);
    if (flip())
      I->setHasNoSignedWrap();
    if (flip())
      I->setHasNoUnsignedWrap();
    return I;
  }
  case 1: {
    auto *I = BinaryOperator::Create(BinaryOperator::Sub, LHS, RHS, "", BB);
    if (flip())
      I->setHasNoSignedWrap();
    if (flip())
      I->setHasNoUnsignedWrap();
    return I;
  }
  case 2: {
    auto *I = BinaryOperator::Create(BinaryOperator::Mul, LHS, RHS, "", BB);
    if (flip())
      I->setHasNoSignedWrap();
    if (flip())
      I->setHasNoUnsignedWrap();
    return I;
  }
  case 3: {
    auto *I = BinaryOperator::Create(BinaryOperator::UDiv, LHS, RHS, "", BB);
    if (flip())
      I->setIsExact();
    return I;
  }
  case 4: {
    auto *I = BinaryOperator::Create(BinaryOperator::SDiv, LHS, RHS, "", BB);
    if (flip())
      I->setIsExact();
    return I;
  }
  case 5:
    return BinaryOperator::Create(BinaryOperator::URem, LHS, RHS, "", BB);
  case 6:
    return BinaryOperator::Create(BinaryOperator::SRem, LHS, RHS, "", BB);
  case 7: {
    auto *I = BinaryOperator::Create(BinaryOperator::Shl, LHS, RHS, "", BB);
    if (flip())
      I->setHasNoSignedWrap();
    if (flip())
      I->setHasNoUnsignedWrap();
    return I;
  }
  case 8: {
    auto *I = BinaryOperator::Create(BinaryOperator::LShr, LHS, RHS, "", BB);
    if (flip())
      I->setIsExact();
    return I;
  }
  case 9: {
    auto *I = BinaryOperator::Create(BinaryOperator::AShr, LHS, RHS, "", BB);
    if (flip())
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

CmpInst::Predicate randomPred() {
  return (CmpInst::Predicate)(
      CmpInst::FIRST_ICMP_PREDICATE +
      choose(CmpInst::LAST_ICMP_PREDICATE + 1 - CmpInst::FIRST_ICMP_PREDICATE));
}

Value *adapt(Value *Val, Type *Ty, BasicBlock *BB,
             const Twine &Name = Twine()) {
  if (choose(15) == 0)
    Val = new FreezeInst(Val, "", BB);
  auto Width = Val->getType()->getIntegerBitWidth();
  auto DesiredWidth = Ty->getIntegerBitWidth();
  if (DesiredWidth == Width)
    return Val;
  if (DesiredWidth < Width)
    return new TruncInst(Val, Ty, Name, BB);
  if (flip())
    return new SExtInst(Val, Ty, Name, BB);
  else
    return new ZExtInst(Val, Ty, Name, BB);
}

// uniformly chosen 16-bit constant -- we'll get them all eventually
APInt uniform16(int Width) {
  return APInt(Width, choose(0xFFFF + 1));
}

// uniformly choose a Hamming weight and then uniformly select a value
// having that weight (but only the bottom half of weights, we'll
// randomly flip the bits later)
APInt hamming(int Width) {
  auto Weight = 1 + choose((Width / 2) - 1);
  APInt I(Width, 0);
  int Remaining = Weight;
  while (Remaining > 0) {
    auto Bit = choose(Width);
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
    auto loBit = choose(Width - 1);
    auto hiBit = loBit + 1 + choose(Width / 4);
    if (hiBit > Width)
      hiBit = Width;
    I.setBits(loBit, hiBit);
  } while (flip());
  return I;
}

// numbers of the form ±2^i ± 2^j ± 2^k
APInt sumOfPowers(int Width) {
  APInt I = APInt(Width, 0);
  do {
    APInt X(Width, 0);
    X.setBit(choose(Width));
    I += flip() ? X : -X;
  } while (flip());
  return I;
}

// uniform random choice from the entire range
APInt uniformInt(int Width) {
  uniform_int_distribution<unsigned long> Dist(
      0, std::numeric_limits<unsigned long>::max());
  return APInt(Width, Dist(*Rand));
}

// chosen values
APInt selectedInt(int Width) {
  switch (choose(5)) {
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
    return flip() ? uniformInt(Width) : selectedInt(Width);

  switch (choose(6)) {
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

Constant *randomInt(Type *Ty, LLVMContext &Ctx, vector<vector<APInt>> &Pool) {
  const auto Width = Ty->getIntegerBitWidth();
  auto &P = Pool.at(Width);

  if (P.size() > 0 && flip()) {
    auto I = P.at(choose(P.size()));
    switch (choose(3)) {
    case 0: {
      APInt Delta(Width, choose(8) + 1);
      I += flip() ? Delta : -Delta;
      break;
    }
    case 1:
      I ^= APInt(Width, 1UL << choose(Width));
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
    switch (choose(5)) {
      case 0: I = ~I; break;
      case 1: I = I + 1; break;
      case 2: I = I - 1; break;
      default: break;
    }
    P.push_back(I);
    return ConstantInt::get(Ty, I);
  }
}

Value *getVal(Type *Ty, const vector<Value *> &Vals,
              vector<vector<APInt>> &Pool, BasicBlock *BB) {
  int num_vals = Vals.size();
  int idx = choose(1 + num_vals);
  if (idx == num_vals)
    return randomInt(Ty, BB->getContext(), Pool);
  else
    return adapt(Vals[idx], Ty, BB);
}

Value *getRandomVal(unsigned Upto, Type *Ty, const vector<Value *> &Vals,
                    vector<vector<APInt>> &Pool) {
  const auto Width = Ty->getIntegerBitWidth();
  auto &P = Pool.at(Width);
  APInt I(Width, choose(Upto));
  P.push_back(I);
  return ConstantInt::get(Ty, I);
}

enum class WidthPolicy { Wild = 0, Chosen, AllOne, Mixed1, Mixed2, Mixed3 };

const array<int, 5> ChosenIntWidths{1, 8, 16, 32, 64};
int SavedWidth;

int getWidth(WidthPolicy P) {
  switch (P) {
  case WidthPolicy::Wild:
    return 1 + choose(MaxIntWidth);
  case WidthPolicy::Chosen: {
    int W;
    do {
      W = ChosenIntWidths[choose(ChosenIntWidths.size())];
    } while (W > MaxIntWidth);
    return W;
  }
  case WidthPolicy::AllOne:
    return SavedWidth;
  case WidthPolicy::Mixed1:
    if (flip())
      return getWidth(WidthPolicy::Wild);
    else
      return getWidth(WidthPolicy::Chosen);
  case WidthPolicy::Mixed2:
    if (flip())
      return SavedWidth;
    else
      return getWidth(WidthPolicy::Chosen);
  case WidthPolicy::Mixed3:
    if (flip())
      return getWidth(WidthPolicy::Wild);
    else
      return SavedWidth;
  default:
    assert(false);
  }
}

Value *genUnaryIntrinsic(Type *Ty, vector<Value *> &Vals,
                         vector<vector<APInt>> &Pool, BasicBlock *BB,
                         const WidthPolicy WP) {
  auto *M = BB->getModule();
  auto &Ctx = BB->getContext();
  auto *Arg = getVal(Ty, Vals, Pool, BB);

  Intrinsic::ID Op;
again:
  switch (choose(6)) {
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
    // FIXME!!! ctpop not supported in ARM lifter yet
    if (opt_backend_tv)
      goto again;
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
    Args.push_back(ConstantInt::get(Type::getInt1Ty(Ctx), flip() ? 1 : 0));

  auto Decl = Intrinsic::getDeclaration(M, Op, Ty);
  auto *I = CallInst::Create(Decl, Args, "", BB);
  return I;
}

Value *genBinaryIntrinsic(Type *Ty, vector<Value *> &Vals,
                          vector<vector<APInt>> &Pool, BasicBlock *BB,
                          const WidthPolicy WP) {
  auto *M = BB->getModule();
  auto *LHS = getVal(Ty, Vals, Pool, BB);
  auto *RHS = getVal(Ty, Vals, Pool, BB);

  Intrinsic::ID Op;
  switch (choose(16)) {
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

  auto Decl = Intrinsic::getDeclaration(M, Op, Ty);
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

Value *genTernaryIntrinsic(Type *Ty, vector<Value *> &Vals,
                           vector<vector<APInt>> &Pool, BasicBlock *BB,
                           const WidthPolicy WP) {
  auto *M = BB->getModule();
  auto *A = getVal(Ty, Vals, Pool, BB);
  auto *B = getVal(Ty, Vals, Pool, BB);
  bool force_32bits_imm = false;
  Intrinsic::ID Op;

  switch (choose(6)) {
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
    ? getRandomVal(Ty->getIntegerBitWidth(), Type::getInt32Ty(BB->getContext()),
                   Vals, Pool)
    : getVal(Ty, Vals, Pool, BB);

  auto Decl = Intrinsic::getDeclaration(M, Op, Ty);
  auto *I = CallInst::Create(Decl, {A, B, C}, "", BB);
  return I;
}

void genInst(vector<Value *> &Vals, vector<vector<APInt>> &Pool, BasicBlock *BB,
             const WidthPolicy WP) {
  auto *Ty = Type::getIntNTy(BB->getContext(), getWidth(WP));
  Value *Val = nullptr;
  switch (choose(4)) {
  case 0: {
    auto *LHS = getVal(Ty, Vals, Pool, BB);
    auto *RHS = getVal(Ty, Vals, Pool, BB);
    Val = new ICmpInst(*BB, randomPred(), LHS, RHS);
  } break;
  case 1: {
    auto *LHS = getVal(Ty, Vals, Pool, BB);
    Value *RHS = getVal(Ty, Vals, Pool, BB);
    auto NarrowWidth = ilog2_ceil(Ty->getIntegerBitWidth(), true);
    auto *NarrowTy = Type::getIntNTy(BB->getContext(), NarrowWidth);
    auto *Mask = ConstantInt::get(Ty, (1UL << NarrowWidth) - 1);
    auto *AltRHS = flip()
                       ? adapt(adapt(RHS, NarrowTy, BB, "mask"), Ty, BB, "mask")
                       : BinaryOperator::Create(BinaryOperator::And, RHS, Mask,
                                                "mask", BB);
    BinaryOperator *BinOp = randomBinop(LHS, PoisonValue::get(Ty), BB);
    Val = BinOp;
    auto Op = BinOp->getOpcode();
    bool isConst = isa<ConstantInt>(RHS);
    // all literal constants and some variable inputs should be
    // truncated to avoid too much poison due to OOB shift exponents
    if ((Op == BinaryOperator::LShr || Op == BinaryOperator::AShr ||
         Op == BinaryOperator::Shl) &&
        (isConst || flip())) {
      BinOp->setOperand(1, AltRHS);
    } else {
      BinOp->setOperand(1, RHS);
    }
  } break;
  case 2: {
    auto *LHS = getVal(Ty, Vals, Pool, BB);
    auto *RHS = getVal(Ty, Vals, Pool, BB);
    auto *Cond = getVal(Type::getInt1Ty(BB->getContext()), Vals, Pool, BB);
    Val = SelectInst::Create(Cond, LHS, RHS, "", BB);
  } break;
  case 3: {
    switch (choose(3)) {
    case 0:
      Val = genUnaryIntrinsic(Ty, Vals, Pool, BB, WP);
      break;
    case 1:
      Val = genBinaryIntrinsic(Ty, Vals, Pool, BB, WP);
      break;
    case 2:
      Val = genTernaryIntrinsic(Ty, Vals, Pool, BB, WP);
      break;
    default:
      assert(false);
    }
  } break;
  default:
    assert(false);
  }
  Vals.push_back(Val);
}

void valueFuzzer(Module *M) {
  WidthPolicy WP = (WidthPolicy)choose(6);
  SavedWidth =
      flip() ? getWidth(WidthPolicy::Wild) : getWidth(WidthPolicy::Chosen);

  auto &Ctx = M->getContext();
  const int NumIntParams = 1 + choose(MaxIntParams);

  auto *RetTy = Type::getIntNTy(Ctx, getWidth(WP));
  vector<Type *> ParamsTy;
  for (int i = 0; i < NumIntParams; ++i)
    ParamsTy.push_back(Type::getIntNTy(Ctx, getWidth(WP)));
  auto *FTy = FunctionType::get(RetTy, ParamsTy, false);
  auto *F = Function::Create(FTy, GlobalValue::ExternalLinkage, 0, "f", M);
  if (flip()) {
    if (flip())
      F->addRetAttr(Attribute::ZExt);
    else
      F->addRetAttr(Attribute::SExt);
  }
  if (choose(4) == 0)
    F->addRetAttr(Attribute::NoUndef);
  auto BB = BasicBlock::Create(Ctx, "", F);

  vector<Value *> Vals;
  for (auto &arg : F->args()) {
    Vals.push_back(&arg);
    if (flip()) {
      if (flip())
        arg.addAttr(Attribute::ZExt);
      else
        arg.addAttr(Attribute::SExt);
    }
    if (choose(4) == 0)
      arg.addAttr(Attribute::NoUndef);
  }

  vector<vector<APInt>> Pool;
  for (int i = 0; i <= MaxIntWidth; ++i)
    Pool.push_back(vector<APInt>());

  // FIXME
  int num_insts = opt_backend_tv ? choose(10) : choose(MaxInsts);

  for (int i = 0; i < num_insts; ++i)
    genInst(Vals, Pool, BB, WP);

  ReturnInst::Create(Ctx, getVal(RetTy, Vals, Pool, BB), BB);
}

void bbFuzzer(Module *M) {
  auto &Ctx = M->getContext();

  const int NumCounters = 1 + choose(MaxCounters);
  const int NumBoolParams = 1 + choose(MaxBoolParams);
  const int NumBBs = 2 + choose(MaxBBs);
  const int Width = 1 + choose(MaxWidth);
  auto *IntTy = Type::getIntNTy(Ctx, Width);

  auto *Callee1Ty = FunctionType::get(Type::getVoidTy(Ctx), {}, false);
  auto *Callee1 =
      Function::Create(Callee1Ty, GlobalValue::ExternalLinkage, 0, "c1", M);

  auto *Callee2Ty = FunctionType::get(Type::getVoidTy(Ctx), {IntTy}, false);
  auto *Callee2 =
      Function::Create(Callee2Ty, GlobalValue::ExternalLinkage, 0, "c2", M);

  vector<Type *> ParamsTy;
  auto *BoolTy = Type::getInt1Ty(Ctx);
  for (int i = 0; i < NumBoolParams; ++i)
    ParamsTy.push_back(BoolTy);
  auto *FTy = FunctionType::get(IntTy, ParamsTy, false);
  auto *F = Function::Create(FTy, GlobalValue::ExternalLinkage, 0, "f", M);
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
    if (choose(10) == 0) {
      if (flip()) {
        auto idx = choose(NumCounters);
        auto *Load = new LoadInst(IntTy, Counters[idx], "", BBs[i]);
        CallInst::Create(Callee2Ty, Callee2, {Load}, "", BBs[i]);
      } else {
        CallInst::Create(Callee1Ty, Callee1, {}, "", BBs[i]);
      }
    } else {
      auto idx = choose(NumCounters);
      auto *Load = new LoadInst(IntTy, Counters[idx], "", BBs[i]);
      auto *Inc =
          BinaryOperator::Create(Instruction::Add, Load, One, "", BBs[i]);
      if (flip())
        Inc->setHasNoUnsignedWrap();
      if (flip())
        Inc->setHasNoSignedWrap();
      new StoreInst(Inc, Counters[idx], BBs[i]);
    }
  }

  for (int i = 0; i < NumBBs; ++i) {
  again:
    switch (choose(4)) {
    case 0: {
      if (i == 0 || (choose(100) > 25))
        goto again;
      auto idx = choose(NumCounters);
      auto *Load = new LoadInst(IntTy, Counters[idx], "", BBs[i]);
      ReturnInst::Create(Ctx, Load, BBs[i]);
    } break;
    case 1: {
      auto *Dest = BBs[1 + choose(NumBBs - 1)];
      BranchInst::Create(Dest, BBs[i]);
    } break;
    case 2: {
      auto *Dest1 = BBs[1 + choose(NumBBs - 1)];
      auto *Dest2 = BBs[1 + choose(NumBBs - 1)];
      Value *Cond = nullptr;
      if (flip()) {
        Cond = Args[FirstBoolParamIdx + choose(NumBoolParams)];
      } else {
        auto *LHS =
            new LoadInst(IntTy, Counters[choose(NumCounters)], "", BBs[i]);
        auto *RHS =
            (flip())
                ? new LoadInst(IntTy, Counters[choose(NumCounters)], "", BBs[i])
                : (Value *)ConstantInt::get(IntTy, choose(20));
        Cond = new ICmpInst(*BBs[i], randomPred(), LHS, RHS);
      }
      BranchInst::Create(Dest1, Dest2, Cond, BBs[i]);
    } break;
    case 3: {
      unsigned long NumCases = 1 + choose(2 * NumBBs);
      auto *Load =
          new LoadInst(IntTy, Counters[choose(NumCounters)], "", BBs[i]);
      auto *Swch = SwitchInst::Create(Load, BBs[1 + choose(NumBBs - 1)],
                                      NumCases, BBs[i]);
      for (unsigned long i = 0; i < NumCases; ++i) {
        if (i >= (1UL << Width))
          break;
        Swch->addCase(ConstantInt::get(IntTy, i), BBs[1 + choose(NumBBs - 1)]);
        if (choose(4) == 0)
          i += choose(4);
      }
    } break;
    default:
      assert(false);
    }
  }

  if (verifyModule(*M, &errs()))
    report_fatal_error("Broken module found, this should not happen");
}

void doit(llvm::Module *M1, llvm::Function *srcFn, Verifier &verifier) {
  lifter::reset();

  // this has to return a fresh function since it rewrites the
  // signature
  srcFn = lifter::adjustSrc(srcFn);
  
  std::unique_ptr<llvm::Module> M2 = std::make_unique<llvm::Module>("M2", M1->getContext());
  M2->setDataLayout(M1->getDataLayout());
  M2->setTargetTriple(M1->getTargetTriple());

  llvm::SmallString<1024> Asm;
  auto AsmBuffer = lifter::generateAsm(*M1, Asm);

  cout << "\n\nAArch64 Assembly:\n\n";
  for (auto it = AsmBuffer->getBuffer().begin(); it != AsmBuffer->getBuffer().end();
       ++it) {
    cout << *it;
  }
  cout << "-------------\n";

  auto [F1, F2] = lifter::liftFunc(M1, M2.get(), srcFn, std::move(AsmBuffer));
  
  auto err = optimize_module(M2.get(), "Oz");
  assert(err.empty());

  verifier.compareFunctions(*F1, *F2);
}

} // namespace

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  PrettyStackTraceProgram X(argc, argv);
  EnableDebugBuffering = true;
  llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  LLVMContext Context;

  std::string Usage =
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

  unique_ptr<Module> M1;

  void (*Fuzzer)(Module *);
  if (opt_fuzzer == "value") {
    Fuzzer = valueFuzzer;
  } else if (opt_fuzzer == "bb") {
    Fuzzer = bbFuzzer;
  } else {
    *out << "Legal fuzzers are \"value\" and \"bb\".\n\n";
    exit(-1);
  }

  auto DL = DataLayout("e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128");
  const string TTStr("aarch64-linux-gnu");
  auto TT = Triple(TTStr);
  TargetLibraryInfoWrapperPass TLI(TT);

  llvm_util::initializer llvm_util_init(*out, DL);
  smt::smt_initializer smt_init;
  Verifier verifier(TLI, smt_init, *out);
  verifier.quiet = opt_quiet;
  verifier.always_verify = opt_always_verify;
  verifier.print_dot = opt_print_dot;
  verifier.bidirectional = opt_bidirectional;

  initFuzzer();

  for (int rep = 0; rep < opt_num_reps; ++rep) {
    cout << "\n================================================================= (" <<
      rep << ")\n\n";
    
    M1 = make_unique<Module>("fuzz", Context);
    M1.get()->setTargetTriple(TTStr);
    M1.get()->setDataLayout(DL);

    Fuzzer(M1.get());

    if (opt_run_sroa) {
      auto err = optimize_module(M1.get(), "sroa,dse");
      assert(err.empty());
    }

    if (opt_run_dce) {
      auto err = optimize_module(M1.get(), "adce");
      assert(err.empty());
    }

    if (opt_save_ir) {
      stringstream output_fn;
      output_fn << "file_" << rep << ".bc";
      *out << "saving IR as '" << output_fn.str() << "'\n";
      std::error_code EC;
      raw_fd_ostream output_file(output_fn.str(), EC);
      if (EC)
        report_fatal_error("Couldn't open output file, exiting");
      WriteBitcodeToFile(*M1.get(), output_file);
    }

    if (opt_print_ir || opt_backend_tv) {
      out->flush();
      outs() << "------------------------------------------------------\n\n";
      M1.get()->print(outs(), nullptr);
      outs() << "------------------------------------------------------\n\n";
      outs().flush();
    }

    if (opt_skip_alive)
      continue;

    if (opt_backend_tv) {
      llvm::Function *srcFn = nullptr;
      for (auto &F : *M1.get()) {
	if (F.isDeclaration())
	  continue;
	srcFn = &F;
      }
      assert(srcFn != nullptr);
      doit(M1.get(), srcFn, verifier);
    } else {
      unique_ptr<Module> M2 = CloneModule(*M1.get());
      auto err = optimize_module(M2.get(), optPass);
      if (!err.empty()) {
	*out << "Error parsing list of LLVM passes: " << err << '\n';
	return -1;
      }

      auto *F1 = M1->getFunction("f");
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
    }
    M1 = nullptr;
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

  return 0;
}
