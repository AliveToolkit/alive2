// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "cache/cache.h"
#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "tools/mutator-utils/mutator.h"
#include "tools/mutator-utils/mutator_helper.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/TargetParser/Triple.h"
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
llvm::cl::OptionCategory mutatorArgs("Mutator options");

llvm::cl::opt<string> inputFile(llvm::cl::Positional,
                                llvm::cl::desc("<inputFile>"),
                                llvm::cl::Required,
                                llvm::cl::value_desc("filename"),
                                llvm::cl::cat(mutatorArgs));

llvm::cl::opt<string> outputFolder(llvm::cl::Positional,
                                   llvm::cl::desc("<outputFileFolder>"),
                                   llvm::cl::Required,
                                   llvm::cl::value_desc("folder"),
                                   llvm::cl::cat(mutatorArgs));

llvm::cl::opt<long long> randomSeed(
    LLVM_ARGS_PREFIX "seed", llvm::cl::value_desc("overall-seed"),
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc("The overall PRNG seed; it is used to compute an "
                   "individual seed for each mutant, which is part of "
                   "the output. Use this seed to repeat an entire run "
                   "of alive-mutate and -individual-seed to regenerate "
                   "a specific mutant (default=random)."),
    llvm::cl::init(-1));

llvm::cl::opt<long long> individualSeed(
    LLVM_ARGS_PREFIX "individual-seed", llvm::cl::value_desc("individual seed"),
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc("Use this option, along with a specific mutant's "
                   "seed, to regenerate a specific mutant. Be careful "
                   "that all other command line arguments are the same "
                   "as those that were used originally."),
    llvm::cl::init(-1));

llvm::cl::opt<int>
    numCopy(LLVM_ARGS_PREFIX "n", llvm::cl::value_desc("number of mutants"),
            llvm::cl::desc("Number of mutants to generate, before exiting "),
            llvm::cl::cat(mutatorArgs), llvm::cl::init(-1));

llvm::cl::opt<int> timeElapsed(
    LLVM_ARGS_PREFIX "t", llvm::cl::value_desc("seconds"),
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc("Number of seconds alive-mutate will run, before exiting"),
    llvm::cl::init(-1));

llvm::cl::opt<bool> removeUndef(
    LLVM_ARGS_PREFIX "removeUndef", llvm::cl::value_desc("remove undef"),
    llvm::cl::desc("Suppress undef values in mutants (default=false)"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool> randomMutate(
    LLVM_ARGS_PREFIX "randomMutate",
    llvm::cl::value_desc("turn on randomMutate mode"),
    llvm::cl::desc("This mode is turned off by default. Random mutate mode "
                   "will randomly mutate an instruction in the"
                   " function instead of linearly"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool>
    disableAlive(LLVM_ARGS_PREFIX "disableAlive",
                 llvm::cl::value_desc("disable Alive"),
                 llvm::cl::desc("Disable Alive2 verification (default=false)"),
                 llvm::cl::cat(mutatorArgs), llvm::cl::init(false));

llvm::cl::opt<bool> verifyInputModule(
    LLVM_ARGS_PREFIX "verifyInputModule",
    llvm::cl::value_desc("verify the input module"),
    llvm::cl::desc(
        "As a preprocessing step, ensure that each un-mutated function "
        "in the input module can be verified by Alive2. Functions that "
        "cannot be verified will not be mutated later (default=true)"),
    llvm::cl::cat(mutatorArgs), llvm::cl::init(true));

llvm::cl::opt<bool>
    verbose(LLVM_ARGS_PREFIX "v", llvm::cl::value_desc("verbose mode"),
            llvm::cl::desc("Print details about mutations that are "
                           "being performed (default=false)"),
            llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool>
    saveAll(LLVM_ARGS_PREFIX "saveAll",
            llvm::cl::value_desc("save all mutants"),
            llvm::cl::desc("Save mutatnts to disk (default=false)"),
            llvm::cl::cat(mutatorArgs), llvm::cl::init(false));

llvm::cl::opt<bool> onEveryFunction(
    LLVM_ARGS_PREFIX "onEveryFunction",
    llvm::cl::value_desc("instead of mutating a single function, all function "
                         "in the module would be mutated"),
    llvm::cl::desc("When mutating a module, mutate every function in it, "
                   "instead of mutating just one function per iteration "
                   "(default=false)"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool> mutateRangeAttr(
    LLVM_ARGS_PREFIX "range-attr",
    llvm::cl::value_desc(
        "mutate range attribute on function arguments and return values "),
    llvm::cl::desc(
        "When mutating function attributes, add or remove range attribute "
        "(default=false)"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<string> optPass(
    LLVM_ARGS_PREFIX "passes", llvm::cl::value_desc("optimization passes"),
    llvm::cl::desc("Specify which LLVM passes to run (default=O2). "
                   "The syntax is described at "
                   "https://llvm.org/docs/NewPassManager.html#invoking-opt"),
    llvm::cl::cat(alive_cmdargs), llvm::cl::init("O2"));

llvm::cl::opt<int> copyFunctions(
    LLVM_ARGS_PREFIX "duplicate",
    llvm::cl::value_desc("the number of functions duplicated"),
    llvm::cl::desc(
        "duplicate every function in the module with specified number."),
    llvm::cl::init(0));

llvm::cl::opt<int> maxError(
    LLVM_ARGS_PREFIX "maxError",
    llvm::cl::value_desc("the number of max error allowed"),
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc("Specify the max error allowed in running mutators. It "
                   "automatically exits after the error number reached."),
    llvm::cl::init(-1));
} // namespace

llvm::cl::list<size_t> disableSEXT(
    LLVM_ARGS_PREFIX "disable-sigext",
    llvm::cl::value_desc("list of integer width"),
    llvm::cl::desc("option list -- This option would disable adding or "
                   "removing sigext on integer type you specified"),
    llvm::cl::CommaSeparated, llvm::cl::cat(mutatorArgs));

llvm::cl::list<size_t> disableZEXT(
    LLVM_ARGS_PREFIX "disable-zeroext",
    llvm::cl::value_desc("list of integer width"),
    llvm::cl::desc("option list -- This option would disable adding or "
                   "removing zext on integer type you specified"),
    llvm::cl::CommaSeparated, llvm::cl::cat(mutatorArgs));

llvm::cl::list<size_t> disableEXT(
    LLVM_ARGS_PREFIX "disable-ext",
    llvm::cl::value_desc("list of integer width"),
    llvm::cl::desc("option list -- This option would disable all ext "
                   "instructions on integer type you specified"),
    llvm::cl::CommaSeparated);

unique_ptr<Cache> cache;
std::stringstream logStream;
// To eliminate extra verifier construction;
std::optional<llvm::TargetLibraryInfoWrapperPass> TLI;
std::optional<smt::smt_initializer> smt_init;
std::optional<Verifier> verifier;
int maxErrorAllowed = -1;

void initVerifier(llvm::Triple targetTriple) {
  TLI.emplace(targetTriple);

  smt_init.emplace();
  verifier.emplace(TLI.value(), smt_init.value(), logStream);
  verifier->quiet = opt_quiet;
  verifier->always_verify = opt_always_verify;
  verifier->print_dot = opt_print_dot;
  verifier->bidirectional = opt_bidirectional;
}

void destroyVerifier() {
  verifier.reset();
  smt_init.reset();
  TLI.reset();
  logStream.str("");
}

std::string getOutputSrcFilename(int ith);
std::string getOutputLogFilename(int ith);
// This function would detect result from verifier and check log information.
// If there is an unsound, write log and the mutated module.
void tryWriteLog(std::string logPath);
bool isValidOutputPath();

// These function are used for actually running mutations. Both copy mode and
// time mode would call runOnce a couple of times and exit.
int copyMode(std::shared_ptr<llvm::Module> &pm),
    timeMode(std::shared_ptr<llvm::Module> &pm);
void runOnce(int ith, Mutator &mutator);

// We need to verify the input to avoid certain scenarios.
// 1. For those already non-sound functions, we need to skip them.
// 2. Remove TBAA metadata from the module
// 3. Replace undef appeared in the module
// 4. Rename unnamed functions
// 5. Skip functions stored in some function pointers
// 6. Skip functions with the only declaration.
// 7. Reset 'internal' attr on functions (because they might be removed after
// optimization) return true if all functions are invalid
llvm::StringSet<> invalidFunctions;
bool verifyInput(std::shared_ptr<llvm::Module> &pm);

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive-mutate, a stand-alone LLVM IR fuzzer cooperates  with Alive2. Alive2 version: )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see alive-mutate --help for more options,
)EOF";

  llvm::cl::HideUnrelatedOptions(mutatorArgs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);
  // Some initial setting of mutation helpers:
  FunctionAttributeHelper::mutateRangeAttr = mutateRangeAttr;

  auto uni_M1 = openInputFile(Context, inputFile);
  std::shared_ptr M1 = std::move(uni_M1);
  if (!M1.get()) {
    llvm::errs() << "Could not read input file from '" << inputFile << "'\n";
    return -1;
  }

#define ARGS_MODULE_VAR M1
#include "llvm_util/cmd_args_def.h"

  if (outputFolder.back() != '/')
    outputFolder += '/';

  if (randomSeed >= 0) {
    Random::setMasterSeed((unsigned)randomSeed);
  }
  if (randomSeed == -1 && individualSeed >= 0) {
    Random::setSeed((unsigned)individualSeed);
    numCopy = 1;
  }

  if (maxError != -1) {
    maxErrorAllowed = maxError;
  }

  if (numCopy < 0 && timeElapsed < 0) {
    llvm::errs() << "Please specify either number of copies or running time!\n";
    return -1;
  } else if (!isValidOutputPath()) {
    llvm::errs() << "Output folder does not exist!\n";
    return -1;
  }

  if (verifyInput(M1)) {
    llvm::errs() << "All functions cannot pass input check!\n";
    return -1;
  }

  if (invalidFunctions.size() > 0) {
    llvm::errs()
        << "Some functions can't pass input check, those would be skipped. "
           "They might are only function declarations, "
        << (verifyInputModule ? "can't pass alive2 initial checks, " : "")
        << " or stores a function pointer inside\n";
  }

  if (randomSeed == -1 && individualSeed >= 0) {
    llvm::outs() << "Current individual random seed: " << Random::getSeed()
                 << "\n";
  } else {
    llvm::outs() << "Current random seed: " << Random::getMasterSeed() << "\n";
  }
  llvm::outs().flush();

  int totalMutants = 0;
  if (numCopy > 0) {
    totalMutants = copyMode(M1);
  } else if (timeElapsed > 0) {
    totalMutants = timeMode(M1);
  }
  llvm::outs() << "Total mutants generated: " << totalMutants << "\n";
  return 0;
}

bool isValidOutputPath() {
  bool result = filesystem::status(string(outputFolder)).type() ==
                filesystem::file_type::directory;
  return result;
}

std::string getOutputSrcFilename(int ith) {
  static filesystem::path inputPath = filesystem::path(string(inputFile));
  static string templateName = string(outputFolder) + inputPath.stem().string();
  return templateName + to_string(ith) + ".ll";
}

std::string getOutputLogFilename(int ith) {
  return getOutputSrcFilename(ith) + "-log.txt";
}

bool verifyInput(std::shared_ptr<llvm::Module> &M1) {
  mutator_util::removeTBAAMetadata(M1.get());
  if (removeUndef) {
    ModuleMutator mutator(M1, verbose, onEveryFunction);
    std::shared_ptr<llvm::Module> newM1 = CloneModule(*M1);
    mutator.init();
    mutator.removeAllUndefInFunctions();
    M1 = mutator.getModule();
  }

  // Rename unnamed functions
  size_t unnamedFunction = 0;
  for_each(M1->begin(), M1->end(), [&unnamedFunction](llvm::Function &f) {
    if (f.getName().empty()) {
      f.setName(std::string("resetUnnamedFunction") +
                std::to_string(unnamedFunction++));
    }
  });

  // Reset internal attr
  for_each(M1->begin(), M1->end(), [](llvm::Function &f) {
    if (f.getLinkage() == llvm::GlobalValue::LinkageTypes::InternalLinkage) {
      f.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
    if (f.getLinkage() == llvm::GlobalValue::LinkageTypes::LinkOnceAnyLinkage) {
      f.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }
  });

  for (auto fit = M1->begin(); fit != M1->end(); ++fit) {
    if (!fit->isDeclaration() || invalidFunctions.contains(fit->getName())) {
      if (llvm::verifyFunction(*fit, nullptr)) {
        invalidFunctions.insert(fit->getName());
        continue;
      }

      for (auto use_it = fit->use_begin(); use_it != fit->use_end(); use_it++) {
        llvm::Value *user = use_it->getUser();
        if (llvm::StoreInst *inst = dyn_cast<llvm::StoreInst>(user); inst) {
          invalidFunctions.insert(inst->getFunction()->getName());
        }
      }
    }
  }

  auto &DL = M1.get()->getDataLayout();
  llvm_util::initializer llvm_util_init(logStream, DL);

  if (verifyInputModule) {
    unique_ptr<llvm::Module> M2 = CloneModule(*M1);
    llvm_util::optimize_module(M2.get(), optPass);

    llvm::Triple targetTriple(M1.get()->getTargetTriple());
    initVerifier(targetTriple);

    size_t incorrect_count = 0;
    for (auto fit = M1->begin(); fit != M1->end(); ++fit) {
      if (!fit->isDeclaration() || invalidFunctions.contains(fit->getName())) {
        llvm::Function *f2 = M2->getFunction(fit->getName());
        if (f2 == nullptr || f2->isDeclaration()) {
          llvm::errs() << "Function: " << fit->getName()
                       << " can't be found in the optimized module\n";
          invalidFunctions.insert(fit->getName());
          continue;
        }
        verifier->compareFunctions(*fit, *f2);
        // FIX ME: need update
        logStream.str("");
        // equals to 0 means not correct
        if (verifier->num_correct == 0) {
          invalidFunctions.insert(fit->getName());
          if (incorrect_count <= 10) {
            llvm::errs() << "Function: " << fit->getName()
                         << " failed in alive2 initial checks.\n";
            ++incorrect_count;
          }
          verifier->num_correct = 0;
        }
      }
    }

    destroyVerifier();
  }
  return invalidFunctions.size() == M1->size();
}

int copyMode(std::shared_ptr<llvm::Module> &pm) {
  if (copyFunctions != 0) {
    mutator_util::propagateFunctionsInModule(pm.get(), copyFunctions);
  }
  std::unique_ptr<Mutator> mutator = std::make_unique<ModuleMutator>(
      pm, invalidFunctions, verbose, onEveryFunction, randomMutate);
  if (bool init = mutator->init(); init) {
    // Eliminate the influence of verifyInput
    if (randomSeed == -1 && individualSeed >= 0) {
      Random::setSeed((unsigned)individualSeed);
    }
    for (int i = 0; i < numCopy; ++i) {
      if (verbose) {
        std::cout << "Running " << i << "th copies." << std::endl;
      }
      if (individualSeed == -1) {
        Random::setSeed(Random::getRandomUnsignedFromMaster());
      }
      runOnce(i, *mutator);
    }
  } else {
    cerr << "Cannot find any locations to mutate, " + inputFile + " skipped!\n";
    return 0;
  }
  return numCopy;
}

int timeMode(std::shared_ptr<llvm::Module> &pm) {
  if (copyFunctions != 0) {
    mutator_util::propagateFunctionsInModule(pm.get(), copyFunctions);
  }
  std::unique_ptr<Mutator> mutator = std::make_unique<ModuleMutator>(
      pm, invalidFunctions, verbose, onEveryFunction, randomMutate);
  bool init = mutator->init();
  if (!init) {
    cerr << "Cannot find any location to mutate, " + inputFile + " skipped\n";
    return 0;
  }
  std::chrono::duration<double> sum = std::chrono::duration<double>::zero();
  int cnt = 1;
  if (randomSeed == -1 && individualSeed >= 0) {
    Random::setSeed((unsigned)individualSeed);
  }
  while (sum.count() < timeElapsed) {
    if (individualSeed == -1) {
      Random::setSeed(Random::getRandomUnsignedFromMaster());
    }
    auto t_start = std::chrono::high_resolution_clock::now();
    runOnce(cnt, *mutator);

    auto t_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> cur = t_end - t_start;
    if (verbose) {
      std::cout << "Generated " + to_string(cnt) + "th copies in " +
                       to_string((cur).count()) + " seconds\n";
    }
    sum += cur;
    ++cnt;
  }
  return cnt;
}

void runOnce(int ith, Mutator &mutator) {
  mutator.mutateModule(getOutputSrcFilename(ith));

  bool hasSaved = false;
  if (saveAll) {
    mutator.saveModule(getOutputSrcFilename(ith));
    hasSaved = true;
  }

  auto M1 = mutator.getModule();

  if (!disableAlive) {
    if (!verifier.has_value()) {
      llvm::Triple targetTriple(M1.get()->getTargetTriple());
      initVerifier(targetTriple);
    }
  }

  const string optFunc = mutator.getCurrentFunction();
  bool shouldLog = false;

  if (llvm::Function *pf1 = M1->getFunction(optFunc);
      !disableAlive && pf1 != nullptr) {
    if (!pf1->isDeclaration()) {
      std::unique_ptr<llvm::Module> M2 = llvm::CloneModule(*M1);
      llvm_util::optimize_module(M2.get(), optPass);
      llvm::Function *pf2 = M2->getFunction(pf1->getName());
      if (pf2 != nullptr) {
        verifier->compareFunctions(*pf1, *pf2);
      }
      if (verifier->num_correct == 0) {
        shouldLog = true;
      }
    }
  }

  if (shouldLog) {
    if (!hasSaved) {
      mutator.saveModule(getOutputSrcFilename(ith));
    }
    std::ofstream logFile(getOutputLogFilename(ith));
    assert(logFile.is_open());
    logFile << "Current seed: " << Random::getSeed() << "\n";
    logFile << "Source file:" << M1->getSourceFileName() << "\n";
    logFile << logStream.rdbuf();
    logStream.str("");
    if (verifier->num_errors != 0) {
      if (maxErrorAllowed == 0) {
        llvm::errs() << "Max number of errors reached. Program ends.\n";
        exit(1);
      }
      --maxErrorAllowed;
    }
  }
}
