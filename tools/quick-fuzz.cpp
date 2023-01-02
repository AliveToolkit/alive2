// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

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

#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

#define LLVM_ARGS_PREFIX ""
#define ARGS_SRC_TGT
#define ARGS_REFINEMENT
#include "llvm_util/cmd_args_list.h"

namespace {

llvm::cl::opt<long> opt_rand_seed(
    LLVM_ARGS_PREFIX "seed",
    llvm::cl::desc("Random seed (default = different every time)"),
    llvm::cl::cat(alive_cmdargs), llvm::cl::init(0));

llvm::cl::opt<long> opt_num_reps(
    LLVM_ARGS_PREFIX "num-reps",
    llvm::cl::desc(
        "Number of times to generate and test a function (default=10)"),
    llvm::cl::cat(alive_cmdargs), llvm::cl::init(10));

llvm::cl::opt<bool>
    opt_skip_alive(LLVM_ARGS_PREFIX "skip-alive",
                   llvm::cl::desc("Generate IR but then don't invoke Alive"),
                   llvm::cl::cat(alive_cmdargs), llvm::cl::init(false));

llvm::cl::opt<bool> opt_run_sroa(
    LLVM_ARGS_PREFIX "run-sroa",
    llvm::cl::desc(
        "Run SROA before llvm2alive, this reduces timeouts by reducing "
        "the load on Alive's memory model"),
    llvm::cl::cat(alive_cmdargs), llvm::cl::init(false));

llvm::cl::opt<string> optPass(
    LLVM_ARGS_PREFIX "passes", llvm::cl::value_desc("optimization passes"),
    llvm::cl::desc("Specify which LLVM passes to run (default=O2). "
                   "The syntax is described at "
                   "https://llvm.org/docs/NewPassManager.html#invoking-opt"),
    llvm::cl::cat(alive_cmdargs), llvm::cl::init("O2"));

// FIXME we might want to be able to specify these on the command
// line, but these should be pretty good defaults
const int MaxBBs = 50;
const int MaxWidth = 20;
const int MaxCounters = 16;
const int MaxBoolParams = 16;

std::optional<std::mt19937_64> Rand;

long choose(long Choices) {
  std::uniform_int_distribution<int> Dist(0, Choices - 1);
  return Dist(*Rand);
}

void init() {
  auto Seed = (opt_rand_seed == 0) ? (std::random_device{}()) : opt_rand_seed;
  cout << "seed = " << Seed << endl;
  Rand.emplace(Seed);
}

llvm::CmpInst::Predicate random_pred() {
  return (llvm::CmpInst::Predicate)(
      llvm::CmpInst::FIRST_ICMP_PREDICATE +
      choose(llvm::CmpInst::LAST_ICMP_PREDICATE + 1 -
             llvm::CmpInst::FIRST_ICMP_PREDICATE));
}

void generate(llvm::Module *M) {
  auto &Ctx = M->getContext();

  const int NumCounters = 1 + choose(MaxCounters);
  const int NumBoolParams = 1 + choose(MaxBoolParams);
  const int NumBBs = 2 + choose(MaxBBs);
  const int Width = 1 + choose(MaxWidth);
  auto *IntTy = llvm::Type::getIntNTy(Ctx, Width);

  auto *Callee1Ty =
      llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {}, false);
  auto *Callee1 = llvm::Function::Create(
      Callee1Ty, llvm::GlobalValue::ExternalLinkage, 0, "c1", M);

  auto *Callee2Ty =
      llvm::FunctionType::get(llvm::Type::getVoidTy(Ctx), {IntTy}, false);
  auto *Callee2 = llvm::Function::Create(
      Callee2Ty, llvm::GlobalValue::ExternalLinkage, 0, "c2", M);

  vector<llvm::Type *> ParamsTy;
  auto *BoolTy = llvm::Type::getInt1Ty(Ctx);
  for (int i = 0; i < NumBoolParams; ++i)
    ParamsTy.push_back(BoolTy);
  auto *FTy = llvm::FunctionType::get(IntTy, ParamsTy, false);
  auto *F = llvm::Function::Create(FTy, llvm::GlobalValue::ExternalLinkage, 0,
                                   "f", M);
  vector<llvm::Argument *> Args;
  for (auto &arg : F->args())
    Args.push_back(&arg);
  int FirstBoolParamIdx = 0;

  vector<llvm::BasicBlock *> BBs;
  for (int i = 0; i < NumBBs; ++i)
    BBs.push_back(llvm::BasicBlock::Create(Ctx, "", F));

  vector<llvm::Value *> Counters;
  auto *Zero = llvm::ConstantInt::get(IntTy, 0);
  auto *One = llvm::ConstantInt::get(IntTy, 1);
  for (int i = 0; i < NumCounters; ++i) {
    Counters.push_back(new llvm::AllocaInst(IntTy, 0, One, "", BBs[0]));
    new llvm::StoreInst(Zero, Counters[i], BBs[0]);
  }

  for (int i = 0; i < NumBBs; ++i) {
    if (choose(10) == 0) {
      if (choose(2) == 0) {
        auto idx = choose(NumCounters);
        auto *Load = new llvm::LoadInst(IntTy, Counters[idx], "", BBs[i]);
        llvm::CallInst::Create(Callee2Ty, Callee2, {Load}, "", BBs[i]);
      } else {
        llvm::CallInst::Create(Callee1Ty, Callee1, {}, "", BBs[i]);
      }
    } else {
      auto idx = choose(NumCounters);
      auto *Load = new llvm::LoadInst(IntTy, Counters[idx], "", BBs[i]);
      auto *Inc = llvm::BinaryOperator::Create(llvm::Instruction::Add, Load,
                                               One, "", BBs[i]);
      if (choose(2) == 0)
        Inc->setHasNoUnsignedWrap();
      if (choose(2) == 0)
        Inc->setHasNoSignedWrap();
      new llvm::StoreInst(Inc, Counters[idx], BBs[i]);
    }
  }

  for (int i = 0; i < NumBBs; ++i) {
  again:
    switch (choose(4)) {
    case 0: {
      if (i == 0 || (choose(100) > 25))
        goto again;
      auto idx = choose(NumCounters);
      auto *Load = new llvm::LoadInst(IntTy, Counters[idx], "", BBs[i]);
      llvm::ReturnInst::Create(Ctx, Load, BBs[i]);
    } break;
    case 1: {
      auto *Dest = BBs[1 + choose(NumBBs - 1)];
      llvm::BranchInst::Create(Dest, BBs[i]);
    } break;
    case 2: {
      auto *Dest1 = BBs[1 + choose(NumBBs - 1)];
      auto *Dest2 = BBs[1 + choose(NumBBs - 1)];
      llvm::Value *Cond = nullptr;
      if (choose(2) == 0) {
        Cond = Args[FirstBoolParamIdx + choose(NumBoolParams)];
      } else {
        auto *LHS = new llvm::LoadInst(IntTy, Counters[choose(NumCounters)], "",
                                       BBs[i]);
        auto *RHS =
            (choose(2) == 0)
                ? new llvm::LoadInst(IntTy, Counters[choose(NumCounters)], "",
                                     BBs[i])
                : (llvm::Value *)llvm::ConstantInt::get(IntTy, choose(20));
        Cond = new llvm::ICmpInst(*BBs[i], random_pred(), LHS, RHS);
      }
      llvm::BranchInst::Create(Dest1, Dest2, Cond, BBs[i]);
    } break;
    case 3: {
      unsigned long NumCases = 1 + choose(2 * NumBBs);
      auto *Load =
          new llvm::LoadInst(IntTy, Counters[choose(NumCounters)], "", BBs[i]);
      auto *Swch = llvm::SwitchInst::Create(Load, BBs[1 + choose(NumBBs - 1)],
                                            NumCases, BBs[i]);
      for (unsigned long i = 0; i < NumCases; ++i) {
        if (i >= (1UL << Width))
          break;
        Swch->addCase(llvm::ConstantInt::get(IntTy, i),
                      BBs[1 + choose(NumBBs - 1)]);
        if (choose(4) == 0)
          i += choose(4);
      }
    } break;
    default:
      assert(false);
    }
  }

  if (llvm::verifyModule(*M, &llvm::errs()))
    llvm::report_fatal_error("Broken module found, this should not happen");
}

} // namespace

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive2 simple generative fuzzer:
version )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see quick-fuzz --version for LLVM version info,

This program stress-tests LLVM and Alive2 by performing randomized
generation of LLVM functions, optimizing them, and then checking
refinement.

It currently contains a single, simple generator that exercises loop
and control flow optimizations, but additional generators are planned.

The recommended workflow is to run quick-fuzz until it finds an issue,
and then re-run with the same seed and also the --save-ir command line
option, in order to get a standalone test case, which can then be
reduced using llvm-reduce.
)EOF";

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  unique_ptr<llvm::Module> MDummy;
#define ARGS_MODULE_VAR MDummy
#include "llvm_util/cmd_args_def.h"

  init();

  llvm::Module M1("fuzz", Context);
  auto &DL = M1.getDataLayout();
  llvm::Triple targetTriple(M1.getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass TLI(targetTriple);

  llvm_util::initializer llvm_util_init(*out, DL);
  smt::smt_initializer smt_init;
  Verifier verifier(TLI, smt_init, *out);
  verifier.quiet = opt_quiet;
  verifier.always_verify = opt_always_verify;
  verifier.print_dot = opt_print_dot;
  verifier.bidirectional = opt_bidirectional;

  for (int rep = 0; rep < opt_num_reps; ++rep) {
    M1.dropAllReferences();
    generate(&M1);

    if (opt_run_sroa) {
      auto err = optimize_module(&M1, "sroa,dse");
      assert(err.empty());
    }

    if (opt_save_ir) {
      stringstream output_fn;
      output_fn << "file_" << rep << ".bc";
      *out << "saving IR as '" << output_fn.str() << "'\n";
      std::error_code EC;
      llvm::raw_fd_ostream output_file(output_fn.str(), EC);
      if (EC)
        llvm::report_fatal_error("Couldn't open output file, exiting");
      llvm::WriteBitcodeToFile(M1, output_file);
    }

    if (opt_skip_alive)
      continue;

    auto M2 = CloneModule(M1);
    auto err = optimize_module(M2.get(), optPass);
    if (!err.empty()) {
      *out << "Error parsing list of LLVM passes: " << err << '\n';
      return -1;
    }

    if (M1.getTargetTriple() != M2.get()->getTargetTriple()) {
      *out << "Modules have different target triples\n";
      return -1;
    }

    auto *F1 = M1.getFunction("f");
    auto *F2 = M2->getFunction("f");
    assert(F1 && F2);

    if (!verifier.compareFunctions(*F1, *F2))
      if (opt_error_fatal)
        goto end;
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
