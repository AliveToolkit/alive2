// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/config.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"

#include <iostream>
#include <utility>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;

static llvm::cl::OptionCategory opt_alive("Alive options");

static llvm::cl::opt<string>
opt_file1(llvm::cl::Positional, llvm::cl::desc("first_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<string>
opt_file2(llvm::cl::Positional, llvm::cl::desc("second_bitcode_file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"),
    llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_disable_undef("disable-undef-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Assume inputs are not undef (default=false)"));

static llvm::cl::opt<bool> opt_disable_poison("disable-poison-input",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Assume inputs are not poison (default=false)"));

static llvm::cl::opt<bool> opt_se_verbose(
    "tv-se-verbose", llvm::cl::desc("Alive: symbolic execution verbose mode"),
    llvm::cl::init(false));

static llvm::cl::opt<unsigned> opt_smt_to(
  "tv-smt-to", llvm::cl::desc("Alive: timeout for SMT queries (default=1000)"),
  llvm::cl::init(1000), llvm::cl::value_desc("ms"), llvm::cl::cat(opt_alive));

static llvm::cl::opt<bool> opt_smt_verbose(
    "tv-smt-verbose", llvm::cl::desc("Alive: SMT verbose mode"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> opt_bidirectional("bidirectional",
    llvm::cl::init(false), llvm::cl::cat(opt_alive),
    llvm::cl::desc("Alive: Run refinement check in both directions"));

static llvm::ExitOnError ExitOnErr;

// adapted from llvm-dis.cpp
static std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                                   string InputFilename) {
  auto MB =
    ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(InputFilename)));
  llvm::SMDiagnostic Diag;
  auto M = getLazyIRModule(move(MB), Diag, Context,
                           /*ShouldLazyLoadMetadata=*/true);
  if (!M) {
    Diag.print("", llvm::errs(), false);
    return 0;
  }
  ExitOnErr(M->materializeAll());
  return M;
}

// adapted from FunctionComparator.cpp
static int cmpNumbers(uint64_t L, uint64_t R) {
  if (L < R) return -1;
  if (L > R) return 1;
  return 0;
}

// adapted from FunctionComparator.cpp
static int cmpTypes(llvm::Type *TyL, llvm::Type *TyR,
                    llvm::Function *FnL, llvm::Function *FnR) {
  llvm::PointerType *PTyL = llvm::dyn_cast<llvm::PointerType>(TyL);
  llvm::PointerType *PTyR = llvm::dyn_cast<llvm::PointerType>(TyR);

  const llvm::DataLayout &DL = FnL->getParent()->getDataLayout();
  if (PTyL && PTyL->getAddressSpace() == 0)
    TyL = DL.getIntPtrType(TyL);
  if (PTyR && PTyR->getAddressSpace() == 0)
    TyR = DL.getIntPtrType(TyR);

  if (TyL == TyR)
    return 0;

  if (int Res = cmpNumbers(TyL->getTypeID(), TyR->getTypeID()))
    return Res;

  switch (TyL->getTypeID()) {
  default:
    llvm_unreachable("Unknown type!");
  case llvm::Type::IntegerTyID:
    return cmpNumbers(llvm::cast<llvm::IntegerType>(TyL)->getBitWidth(),
                      llvm::cast<llvm::IntegerType>(TyR)->getBitWidth());
  // TyL == TyR would have returned true earlier, because types are uniqued.
  case llvm::Type::VoidTyID:
  case llvm::Type::FloatTyID:
  case llvm::Type::DoubleTyID:
  case llvm::Type::X86_FP80TyID:
  case llvm::Type::FP128TyID:
  case llvm::Type::PPC_FP128TyID:
  case llvm::Type::LabelTyID:
  case llvm::Type::MetadataTyID:
  case llvm::Type::TokenTyID:
    return 0;

  case llvm::Type::PointerTyID:
    assert(PTyL && PTyR && "Both types must be pointers here.");
    return cmpNumbers(PTyL->getAddressSpace(), PTyR->getAddressSpace());

  case llvm::Type::StructTyID: {
    llvm::StructType *STyL = llvm::cast<llvm::StructType>(TyL);
    llvm::StructType *STyR = llvm::cast<llvm::StructType>(TyR);
    if (STyL->getNumElements() != STyR->getNumElements())
      return cmpNumbers(STyL->getNumElements(), STyR->getNumElements());

    if (STyL->isPacked() != STyR->isPacked())
      return cmpNumbers(STyL->isPacked(), STyR->isPacked());

    for (unsigned i = 0, e = STyL->getNumElements(); i != e; ++i) {
      if (int Res = cmpTypes(STyL->getElementType(i), STyR->getElementType(i), FnL, FnR))
        return Res;
    }
    return 0;
  }

  case llvm::Type::FunctionTyID: {
    llvm::FunctionType *FTyL = llvm::cast<llvm::FunctionType>(TyL);
    llvm::FunctionType *FTyR = llvm::cast<llvm::FunctionType>(TyR);
    if (FTyL->getNumParams() != FTyR->getNumParams())
      return cmpNumbers(FTyL->getNumParams(), FTyR->getNumParams());

    if (FTyL->isVarArg() != FTyR->isVarArg())
      return cmpNumbers(FTyL->isVarArg(), FTyR->isVarArg());

    if (int Res = cmpTypes(FTyL->getReturnType(), FTyR->getReturnType(), FnL, FnR))
      return Res;

    for (unsigned i = 0, e = FTyL->getNumParams(); i != e; ++i) {
      if (int Res = cmpTypes(FTyL->getParamType(i), FTyR->getParamType(i), FnL, FnR))
        return Res;
    }
    return 0;
  }

  case llvm::Type::ArrayTyID:
  case llvm::Type::VectorTyID: {
    auto *STyL = llvm::cast<llvm::SequentialType>(TyL);
    auto *STyR = llvm::cast<llvm::SequentialType>(TyR);
    if (STyL->getNumElements() != STyR->getNumElements())
      return cmpNumbers(STyL->getNumElements(), STyR->getNumElements());
    return cmpTypes(STyL->getElementType(), STyR->getElementType(), FnL, FnR);
  }
  }
}

static optional<smt::smt_initializer> smt_init;

static bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                             llvm::Triple &targetTriple, unsigned &goodCount,
                             unsigned &badCount, unsigned &errorCount) {
  if (cmpTypes(F1.getFunctionType(), F2.getFunctionType(), &F1, &F2)) {
    cerr << "Only functions with identical signatures can be checked\n";
    ++errorCount;
    return true;
  }

  TransformPrintOpts print_opts;

  auto Func1 = llvm2alive(F1, llvm::TargetLibraryInfoWrapperPass(targetTriple).getTLI(F1));
  if (!Func1) {
    cerr << "Could not translate '" + (std::string)F1.getName() + "' to Alive IR\n";
    ++errorCount;
    return true;
  }

  auto Func2 = llvm2alive(F2, llvm::TargetLibraryInfoWrapperPass(targetTriple).getTLI(F2));
  if (!Func2) {
    cerr << "Could not translate '" + (std::string)F2.getName() + "' to Alive IR\n";
    ++errorCount;
    return true;
  }

  smt_init->reset();
  Transform t;
  t.src = move(*Func1);
  t.tgt = move(*Func2);
  TransformVerify verifier(t, false);
  t.print(cout, print_opts);

  {
    auto types = verifier.getTypings();
    if (!types) {
      cerr << "Transformation doesn't verify!\n"
              "ERROR: program doesn't type check!\n\n";
      ++errorCount;
      return true;
    }
    assert(types.hasSingleTyping());
  }

  Errors errs = verifier.verify();
  bool result(errs);
  if (result) {
    cerr << "Transformation doesn't verify!\n" << errs << endl;
    ++badCount;
  } else {
    cerr << "Transformation seems to be correct!\n\n";
    ++goodCount;
  }

  if (opt_bidirectional) {
    smt_init->reset();
    Transform t2;
    t2.src = move(t.tgt);
    t2.tgt = move(t.src);
    TransformVerify verifier2(t2, false);
    t2.print(cout, print_opts);

    if (Errors errs2 = verifier2.verify()) {
      cerr << "Reverse transformation doesn't verify!\n" << errs2 << endl;
    } else {
      cerr << "Reverse transformation seems to be correct!\n\n";
      if (!result)
        cerr << "These functions are equivalent.\n\n";
    }
  }

  return result;
}

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown;  // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  llvm::cl::ParseCommandLineOptions(argc, argv,
                                  "Alive2 stand-alone translation validator\n");

  smt::solver_print_queries(opt_smt_verbose);
  smt::solver_tactic_verbose(false);
  smt::set_query_timeout(to_string(opt_smt_to));
  smt::set_memory_limit(1024 * 1024 * 1024);
  //config::skip_smt = opt_smt_skip;
  config::symexec_print_each_value = opt_se_verbose;
  config::disable_undef_input = opt_disable_undef;
  config::disable_poison_input = opt_disable_poison;

  auto M1 = openInputFile(Context, opt_file1);
  if (!M1.get())
    llvm::report_fatal_error(
      "Could not read bitcode from '" + opt_file1 + "'");

  auto M2 = openInputFile(Context, opt_file2);
  if (!M2.get())
    llvm::report_fatal_error(
      "Could not read bitcode from '" + opt_file2 + "'");

  if (M1.get()->getTargetTriple() != M2.get()->getTargetTriple())
    llvm::report_fatal_error("Modules have different target triple");

  auto &DL = M1.get()->getDataLayout();
  auto targetTriple = llvm::Triple(M1.get()->getTargetTriple());

  llvm_util::initializer llvm_util_init(cerr, DL);
  smt_init.emplace();

  bool result = false;
  unsigned goodCount = 0, badCount = 0, errorCount = 0;
  // FIXME: quadratic, may not be suitable for very large modules
  // emitted by opt-fuzz
  for (auto &F1 : *M1.get()) {
    std::string s = F1.getName();
    for (auto &F2 : *M2.get()) {
      if (F1.getName().equals(F2.getName()))
        result |= compareFunctions(F1, F2, targetTriple, goodCount,
                                   badCount, errorCount);
    }
  }

  cerr << "Summary:\n";
  cerr << "  " << goodCount << " correct transformations\n";
  cerr << "  " << badCount << " incorrect transformations\n";
  cerr << "  " << errorCount << " errors\n";

  smt_init.reset();

  return result;
}
