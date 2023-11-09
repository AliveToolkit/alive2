// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "cache/cache.h"
#include "llvm_util/compare.h"
#include "llvm_util/llvm2alive.h"
#include "llvm_util/llvm_optimizer.h"
#include "llvm_util/utils.h"
#include "smt/smt.h"
#include "tools/mutator-utils/mutator.h"
#include "tools/transform.h"
#include "util/version.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
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
    LLVM_ARGS_PREFIX "s",
    llvm::cl::value_desc("specify the seed of the random number generator"),
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc(
        "specify the seed of the random number generator. It will "
        "set the seed of master RNG if masterRNG argument is specified"),
    llvm::cl::init(-1));

llvm::cl::opt<int> numCopy(LLVM_ARGS_PREFIX "n",
                           llvm::cl::value_desc("number of mutants"),
                           llvm::cl::desc("specify the number of mutants"),
                           llvm::cl::cat(mutatorArgs), llvm::cl::init(-1));

llvm::cl::opt<int> timeElapsed(
    LLVM_ARGS_PREFIX "t", llvm::cl::value_desc("seconds"),
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc("specify how long the mutator should run in seconds"),
    llvm::cl::init(-1));

llvm::cl::opt<bool> removeUndef(
    LLVM_ARGS_PREFIX "removeUndef",
    llvm::cl::value_desc("a flag for turning on removeUndef"),
    llvm::cl::desc("remove all undef in all functions in the input module"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool> masterRNG(
    LLVM_ARGS_PREFIX "masterRNG",
    llvm::cl::value_desc("turn on master RNG mode"),
    llvm::cl::desc("Turn on master RNG mode. Alive-mutate will use a master "
                   "RNG to generate a list of random seeds and use"
                   " one seed for every mutant"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool> randomMutate(
    LLVM_ARGS_PREFIX "randomMutate",
    llvm::cl::value_desc("turn on randomMutate mode"),
    llvm::cl::desc(
        "Random mutate mode will randomly mutate an instruction in the"
        " function instead of linearly"),
    llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool>
    disableAlive(LLVM_ARGS_PREFIX "disableAlive",
                 llvm::cl::value_desc("a flag to disable alive2 verifications"),
                 llvm::cl::desc("don't verify mutants by alive2 and "
                                "save all mutants to output folder"),
                 llvm::cl::cat(mutatorArgs), llvm::cl::init(false));

llvm::cl::opt<bool>
    verbose(LLVM_ARGS_PREFIX "v", llvm::cl::value_desc("verbose mode"),
            llvm::cl::desc("turn on verbose mode, the detail of mutations and "
                           "module will be printed"),
            llvm::cl::cat(mutatorArgs));

llvm::cl::opt<bool> onEveryFunction(
    LLVM_ARGS_PREFIX "onEveryFunction",
    llvm::cl::value_desc("instead of mutating a single function, all function "
                         "in the module would be mutated"),
    llvm::cl::desc("Alive-mutate will perform mutations on every function in "
                   "the module when generating one mutant, otherwise only one "
                   "function will be mutated in one mutant"),
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
    llvm::cl::cat(mutatorArgs),
    llvm::cl::desc(
        "duplicate every function in the module with specified number."),
    llvm::cl::init(0));
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
    llvm::cl::CommaSeparated, llvm::cl::cat(mutatorArgs));

std::vector<unsigned> RNGseeds;
unique_ptr<Cache> cache;
std::stringstream logStream;
// To eliminate extra verifier construction;
std::optional<llvm::TargetLibraryInfoWrapperPass> TLI;
std::optional<smt::smt_initializer> smt_init;
std::optional<Verifier> verifier;

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

  auto uni_M1 = openInputFile(Context, inputFile);
  std::shared_ptr M1 = std::move(uni_M1);
  if (!M1.get()) {
    cerr << "Could not read input file from '" << inputFile << "'\n";
    return -1;
  }

#define ARGS_MODULE_VAR M1
#include "llvm_util/cmd_args_def.h"

  if (outputFolder.back() != '/')
    outputFolder += '/';

  if (randomSeed >= 0) {
    Random::setSeed((unsigned)randomSeed);
    if (masterRNG) {
      assert(numCopy > 0 &&
             "master RNG setting should only be allowed under copy mode!\n");
      RNGseeds.resize(numCopy);
      for (int i = 0; i < numCopy; ++i) {
        RNGseeds[i] = Random::getRandomUnsigned();
      }
    }
  }

  if (numCopy < 0 && timeElapsed < 0) {
    cerr << "Please specify either number of copies or running time!\n";
    return -1;
  } else if (!isValidOutputPath()) {
    cerr << "Output folder does not exist!\n";
    return -1;
  }

  if (verifyInput(M1)) {
    cerr << "All functions cannot pass input check!\n";
    return -1;
  }

  if (invalidFunctions.size() > 0) {
    cerr << "Some functions can't pass input check, those would be skipped\n";
  }

  llvm::outs() << "Current random seed: " << Random::getSeed() << "\n";
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

  unique_ptr<llvm::Module> M2 = CloneModule(*M1);
  llvm_util::optimize_module(M2.get(), optPass);

  auto &DL = M1.get()->getDataLayout();
  llvm_util::initializer llvm_util_init(logStream, DL);
  llvm::Triple targetTriple(M1.get()->getTargetTriple());
  initVerifier(targetTriple);

  for (auto fit = M1->begin(); fit != M1->end(); ++fit) {
    if (!fit->isDeclaration() || invalidFunctions.contains(fit->getName())) {
      llvm::Function *f2 = M2->getFunction(fit->getName());
      verifier->compareFunctions(*fit, *f2);
      // FIX ME: need update
      logStream.str("");
      // equals to 0 means not correct
      if (verifier->num_correct == 0) {
        invalidFunctions.insert(fit->getName());
        verifier->num_correct = 0;
      }
    }
  }
  destroyVerifier();
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
    if (!masterRNG && randomSeed >= 0) {
      Random::setSeed((unsigned)randomSeed);
    }
    for (int i = 0; i < numCopy; ++i) {
      if (verbose) {
        std::cout << "Running " << i << "th copies." << std::endl;
      }
      if (masterRNG) {
        Random::setSeed(RNGseeds[i]);
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
  while (sum.count() < timeElapsed) {
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
      assert(pf2 != nullptr && "pf2 clone failed");
      verifier->compareFunctions(*pf1, *pf2);
      if (verifier->num_correct == 0) {
        shouldLog = true;
      }
    }
  }

  if (shouldLog || disableAlive) {
    mutator.saveModule(getOutputSrcFilename(ith));
    if (!disableAlive) {
      std::ofstream logFile(getOutputLogFilename(ith));
      assert(logFile.is_open());
      logFile << "Current seed: " << Random::getSeed() << "\n";
      logFile << "Source file:" << M1->getSourceFileName() << "\n";
      logFile << logStream.rdbuf();
      logStream.str("");
    }
  }
}
