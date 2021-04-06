// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <fstream>
#include <iostream>
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

llvm::cl::opt<string> opt_file1(llvm::cl::Positional,
  llvm::cl::desc("first_bitcode_file"),
  llvm::cl::Required, llvm::cl::value_desc("filename"),
  llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<string> opt_file2(llvm::cl::Positional,
  llvm::cl::desc("[second_bitcode_file]"),
  llvm::cl::Optional, llvm::cl::value_desc("filename"),
  llvm::cl::cat(alive_cmdargs));

llvm::cl::opt<std::string> opt_src_fn(LLVM_ARGS_PREFIX "src-fn",
  llvm::cl::desc("Name of src function (without @)"),
  llvm::cl::cat(alive_cmdargs), llvm::cl::init("src"));

llvm::cl::opt<std::string> opt_tgt_fn(LLVM_ARGS_PREFIX"tgt-fn",
  llvm::cl::desc("Name of tgt function (without @)"),
  llvm::cl::cat(alive_cmdargs), llvm::cl::init("tgt"));


llvm::ExitOnError ExitOnErr;

// adapted from llvm-dis.cpp
std::unique_ptr<llvm::Module> openInputFile(llvm::LLVMContext &Context,
                                            const string &InputFilename) {
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

optional<smt::smt_initializer> smt_init;

bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                      llvm::TargetLibraryInfoWrapperPass &TLI,
                      unsigned &goodCount, unsigned &badCount,
                      unsigned &errorCount) {
  TransformPrintOpts print_opts;

  auto Func1 = llvm2alive(F1, TLI.getTLI(F1));
  if (!Func1) {
    *out << "ERROR: Could not translate '" << F1.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return true;
  }

  auto Func2 = llvm2alive(F2, TLI.getTLI(F2), Func1->getGlobalVarNames());
  if (!Func2) {
    *out << "ERROR: Could not translate '" << F2.getName().str()
         << "' to Alive IR\n";
    ++errorCount;
    return true;
  }

  if (opt_print_dot) {
    Func1->writeDot("src");
    Func2->writeDot("tgt");
  }

  Transform t;
  t.src = move(*Func1);
  t.tgt = move(*Func2);

  if (!opt_always_verify) {
    stringstream ss1, ss2;
    t.src.print(ss1);
    t.tgt.print(ss2);
    if (ss1.str() == ss2.str()) {
      if (!opt_quiet)
        t.print(*out, print_opts);
      *out << "Transformation seems to be correct! (syntactically equal)\n\n";
      ++goodCount;
      return true;
    }
  }

  smt_init->reset();
  t.preprocess();
  TransformVerify verifier(t, false);
  if (!opt_quiet)
    t.print(*out, print_opts);

  {
    auto types = verifier.getTypings();
    if (!types) {
      *out << "Transformation doesn't verify!\n"
              "ERROR: program doesn't type check!\n\n";
      ++errorCount;
      return false;
    }
    assert(types.hasSingleTyping());
  }

  Errors errs = verifier.verify();
  bool result(errs);
  if (result) {
    if (errs.isUnsound()) {
      *out << "Transformation doesn't verify!\n";
      if (!opt_quiet)
        *out << errs << endl;
      ++badCount;
      return false;
    } else {
      *out << errs << endl;
      ++errorCount;
    }
  } else {
    *out << "Transformation seems to be correct!\n\n";
    ++goodCount;
  }

  if (opt_bidirectional) {
    smt_init->reset();
    Transform t2;
    t2.src = move(t.tgt);
    t2.tgt = move(t.src);
    TransformVerify verifier2(t2, false);
    t2.print(*out, print_opts);

    if (Errors errs2 = verifier2.verify()) {
      *out << "Reverse transformation doesn't verify!\n" << errs2 << endl;
      return false;
    } else {
      *out << "Reverse transformation seems to be correct!\n\n";
      if (!result)
        *out << "These functions are equivalent.\n\n";
    }
  }
  return true;
}

void optimizeModule(llvm::Module *M) {
  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;

  llvm::PassBuilder PB;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  llvm::FunctionPassManager FPM =
    PB.buildFunctionSimplificationPipeline(
      llvm::PassBuilder::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  llvm::ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.run(*M, MAM);
}

llvm::Function *findFunction(llvm::Module &M, const string &FName) {
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    if (FName.compare(F.getName()) != 0)
      continue;
    return &F;
  }
  return 0;
}
}

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive2 stand-alone translation validator:
version )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see alive-tv --version  for LLVM version info,

This program takes either one or two LLVM IR files files as
command-line arguments. Both .bc and .ll files are supported.

If two files are provided, alive-tv checks that functions in the
second file refine functions in the first file, matching up functions
by name. Functions not found in both files are ignored. It is an error
for a function to be found in both files unless they have the same
signature.

If one file is provided, there are two possibilities. If the file
contains a function called "src" and also a function called "tgt",
then alive-tv will determine whether src is refined by tgt. It is an
error if src and tgt do not have the same signature. Otherwise,
alive-tv will optimize the entire module using an optimization
pipeline similar to -O2, and then verify that functions in the
optimized module refine those in the original one. This provides a
convenient way to demonstrate an existing optimizer bug.
)EOF";

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  auto M1 = openInputFile(Context, opt_file1);
  if (!M1.get()) {
    cerr << "Could not read bitcode from '" << opt_file1 << "'\n";
    return -1;
  }

#define ARGS_MODULE_VAR M1
# include "llvm_util/cmd_args_def.h"

  auto &DL = M1.get()->getDataLayout();
  llvm::Triple targetTriple(M1.get()->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass TLI(targetTriple);

  llvm_util::initializer llvm_util_init(*out, DL);
  smt_init.emplace();

  unsigned goodCount = 0, badCount = 0, errorCount = 0;

  unique_ptr<llvm::Module> M2;
  if (opt_file2.empty()) {
    auto SRC = findFunction(*M1, opt_src_fn);
    auto TGT = findFunction(*M1, opt_tgt_fn);
    if (SRC && TGT) {
      compareFunctions(*SRC, *TGT, TLI, goodCount, badCount, errorCount);
      goto end;
    } else {
      M2 = CloneModule(*M1);
      optimizeModule(M2.get());
    }
  } else {
    M2 = openInputFile(Context, opt_file2);
    if (!M2.get()) {
      *out << "Could not read bitcode from '" << opt_file2 << "'\n";
      return -1;
    }
  }

  if (M1.get()->getTargetTriple() != M2.get()->getTargetTriple()) {
    *out << "Modules have different target triples\n";
    return -1;
  }

  // FIXME: quadratic, may not be suitable for very large modules
  // emitted by opt-fuzz
  for (auto &F1 : *M1.get()) {
    if (F1.isDeclaration())
      continue;
    if (!func_names.empty() && !func_names.count(F1.getName().str()))
      continue;
    for (auto &F2 : *M2.get()) {
      if (F2.isDeclaration() || F1.getName() != F2.getName())
        continue;
      if (!compareFunctions(F1, F2, TLI, goodCount, badCount, errorCount))
        if (opt_error_fatal)
          goto end;
      break;
    }
  }

  *out << "Summary:\n"
          "  " << goodCount << " correct transformations\n"
          "  " << badCount << " incorrect transformations\n"
          "  " << errorCount << " Alive2 errors\n";

end:
  if (opt_smt_stats)
    smt::solver_print_stats(*out);

  smt_init.reset();

  if (opt_alias_stats)
    IR::Memory::printAliasStats(*out);

  return errorCount > 0;
}
