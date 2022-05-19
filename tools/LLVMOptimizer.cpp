#include "LLVMOptimizer.h"

using namespace llvm;

// member variable of PassBuilder
/*static llvm::TargetMachine *TM = nullptr;

// member function of PassBuilder
static ModuleInlinerWrapperPass buildInlinerPipeline(OptimizationLevel Level,
                                                     ThinOrFullLTOPhase Phase) {
  assert(false &&
         "this is a PassBuilder Internal function, shouldn't reach there");
}
// The following passes/analyses have custom names, otherwise their name will
// include `(anonymous namespace)`. These are special since they are only for
// testing purposes and don't live in a header file.

namespace {
/// No-op module pass which does nothing.
struct NoOpModulePass : PassInfoMixin<NoOpModulePass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    return PreservedAnalyses::all();
  }
  static StringRef name() {
    return "NoOpModulePass";
  }
};

/// No-op module analysis.
class NoOpModuleAnalysis : public AnalysisInfoMixin<NoOpModuleAnalysis> {
  friend AnalysisInfoMixin<NoOpModuleAnalysis>;
  static AnalysisKey Key;

public:
  struct Result {};
  Result run(Module &, ModuleAnalysisManager &) {
    return Result();
  }
  static StringRef name() {
    return "NoOpModuleAnalysis";
  }
};

/// No-op CGSCC pass which does nothing.
struct NoOpCGSCCPass : PassInfoMixin<NoOpCGSCCPass> {
  PreservedAnalyses run(LazyCallGraph::SCC &C, CGSCCAnalysisManager &,
                        LazyCallGraph &, CGSCCUpdateResult &UR) {
    return PreservedAnalyses::all();
  }
  static StringRef name() {
    return "NoOpCGSCCPass";
  }
};

/// No-op CGSCC analysis.
class NoOpCGSCCAnalysis : public AnalysisInfoMixin<NoOpCGSCCAnalysis> {
  friend AnalysisInfoMixin<NoOpCGSCCAnalysis>;
  static AnalysisKey Key;

public:
  struct Result {};
  Result run(LazyCallGraph::SCC &, CGSCCAnalysisManager &, LazyCallGraph &G) {
    return Result();
  }
  static StringRef name() {
    return "NoOpCGSCCAnalysis";
  }
};

/// No-op function pass which does nothing.
struct NoOpFunctionPass : PassInfoMixin<NoOpFunctionPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    return PreservedAnalyses::all();
  }
  static StringRef name() {
    return "NoOpFunctionPass";
  }
};

/// No-op function analysis.
class NoOpFunctionAnalysis : public AnalysisInfoMixin<NoOpFunctionAnalysis> {
  friend AnalysisInfoMixin<NoOpFunctionAnalysis>;
  static AnalysisKey Key;

public:
  struct Result {};
  Result run(Function &, FunctionAnalysisManager &) {
    return Result();
  }
  static StringRef name() {
    return "NoOpFunctionAnalysis";
  }
};

/// No-op loop nest pass which does nothing.
struct NoOpLoopNestPass : PassInfoMixin<NoOpLoopNestPass> {
  PreservedAnalyses run(LoopNest &L, LoopAnalysisManager &,
                        LoopStandardAnalysisResults &, LPMUpdater &) {
    return PreservedAnalyses::all();
  }
  static StringRef name() {
    return "NoOpLoopNestPass";
  }
};

/// No-op loop pass which does nothing.
struct NoOpLoopPass : PassInfoMixin<NoOpLoopPass> {
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &,
                        LoopStandardAnalysisResults &, LPMUpdater &) {
    return PreservedAnalyses::all();
  }
  static StringRef name() {
    return "NoOpLoopPass";
  }
};

/// No-op loop analysis.
class NoOpLoopAnalysis : public AnalysisInfoMixin<NoOpLoopAnalysis> {
  friend AnalysisInfoMixin<NoOpLoopAnalysis>;
  static AnalysisKey Key;

public:
  struct Result {};
  Result run(Loop &, LoopAnalysisManager &, LoopStandardAnalysisResults &) {
    return Result();
  }
  static StringRef name() {
    return "NoOpLoopAnalysis";
  }
};

/// Parser of parameters for LoopUnroll pass.
LoopUnrollOptions parseLoopUnrollOptions(StringRef Params) {
  LoopUnrollOptions UnrollOpts;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');
    int OptLevel = StringSwitch<int>(ParamName)
                       .Case("O0", 0)
                       .Case("O1", 1)
                       .Case("O2", 2)
                       .Case("O3", 3)
                       .Default(-1);
    if (OptLevel >= 0) {
      UnrollOpts.setOptLevel(OptLevel);
      continue;
    }
    if (ParamName.consume_front("full-unroll-max=")) {
      int Count;
      if (ParamName.getAsInteger(0, Count)) {
        assert(false && "param get as integer failed");
      }
      UnrollOpts.setFullUnrollMaxCount(Count);
      continue;
    }

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "partial") {
      UnrollOpts.setPartial(Enable);
    } else if (ParamName == "peeling") {
      UnrollOpts.setPeeling(Enable);
    } else if (ParamName == "profile-peeling") {
      UnrollOpts.setProfileBasedPeeling(Enable);
    } else if (ParamName == "runtime") {
      UnrollOpts.setRuntime(Enable);
    } else if (ParamName == "upperbound") {
      UnrollOpts.setUpperBound(Enable);
    }
  }
  return UnrollOpts;
}

std::pair<bool, bool> parseLoopUnswitchOptions(StringRef Params) {
  std::pair<bool, bool> Result = {false, true};
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "nontrivial") {
      Result.first = Enable;
    } else if (ParamName == "trivial") {
      Result.second = Enable;
    }
  }
  return Result;
}

LICMOptions parseLICMOptions(StringRef Params) {
  LICMOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "allowspeculation") {
      Result.AllowSpeculation = Enable;
    }
  }
  return Result;
}

bool parseSinglePassOption(StringRef Params, StringRef OptionName,
                           StringRef PassName) {
  bool Result = false;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == OptionName) {
      Result = true;
    } else {
      assert(false &&
             formatv("invalid {1} pass parameter '{0}' ", ParamName, PassName)
                 .str()
                 .c_str());
    }
  }
  return Result;
}

bool parseInlinerPassOptions(StringRef Params) {
  return parseSinglePassOption(Params, "only-mandatory", "InlinerPass");
}

bool parseCoroSplitPassOptions(StringRef Params) {
  return parseSinglePassOption(Params, "reuse-storage", "CoroSplitPass");
}

bool parseEarlyCSEPassOptions(StringRef Params) {
  return parseSinglePassOption(Params, "memssa", "EarlyCSE");
}

bool parseEntryExitInstrumenterPassOptions(StringRef Params) {
  return parseSinglePassOption(Params, "post-inline", "EntryExitInstrumenter");
}

bool parseLoopExtractorPassOptions(StringRef Params) {
  return parseSinglePassOption(Params, "single", "LoopExtractor");
}

bool parseLowerMatrixIntrinsicsPassOptions(StringRef Params) {
  return parseSinglePassOption(Params, "minimal", "LowerMatrixIntrinsics");
}

MemorySanitizerOptions parseMSanPassOptions(StringRef Params) {
  MemorySanitizerOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "recover") {
      Result.Recover = true;
    } else if (ParamName == "kernel") {
      Result.Kernel = true;
    } else if (ParamName.consume_front("track-origins=")) {
      assert(false &&
             formatv("invalid argument to MemorySanitizer pass track-origins "
                     "parameter: '{0}' ",
                     ParamName)
                 .str()
                 .c_str());

    } else if (ParamName == "eager-checks") {
      Result.EagerChecks = true;
    } else {
      assert(false &&
             formatv("invalid MemorySanitizer pass parameter '{0}' ", ParamName)
                 .str()
                 .c_str());
    }
  }
  return Result;
}

SimplifyCFGOptions parseSimplifyCFGOptions(StringRef Params) {
  SimplifyCFGOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "forward-switch-cond") {
      Result.forwardSwitchCondToPhi(Enable);
    } else if (ParamName == "switch-range-to-icmp") {
      Result.convertSwitchRangeToICmp(Enable);
    } else if (ParamName == "switch-to-lookup") {
      Result.convertSwitchToLookupTable(Enable);
    } else if (ParamName == "keep-loops") {
      Result.needCanonicalLoops(Enable);
    } else if (ParamName == "hoist-common-insts") {
      Result.hoistCommonInsts(Enable);
    } else if (ParamName == "sink-common-insts") {
      Result.sinkCommonInsts(Enable);
    } else if (Enable && ParamName.consume_front("bonus-inst-threshold=")) {
      APInt BonusInstThreshold;
      if (ParamName.getAsInteger(0, BonusInstThreshold))
        assert(false &&
               formatv("invalid argument to SimplifyCFG pass bonus-threshold "
                       "parameter: '{0}' ",
                       ParamName)
                   .str()
                   .c_str());
      Result.bonusInstThreshold(BonusInstThreshold.getSExtValue());
    } else {
      assert(false &&
             formatv("invalid SimplifyCFG pass parameter '{0}' ", ParamName)
                 .str()
                 .c_str());
    }
  }
  return Result;
}

LoopVectorizeOptions parseLoopVectorizeOptions(StringRef Params) {
  LoopVectorizeOptions Opts;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "interleave-forced-only") {
      Opts.setInterleaveOnlyWhenForced(Enable);
    } else if (ParamName == "vectorize-forced-only") {
      Opts.setVectorizeOnlyWhenForced(Enable);
    } else {
      assert(false &&
             formatv("invalid LoopVectorize parameter '{0}' ", ParamName)
                 .str()
                 .c_str());
    }
  }
  return Opts;
}

StackLifetime::LivenessType parseStackLifetimeOptions(StringRef Params) {
  StackLifetime::LivenessType Result = StackLifetime::LivenessType::May;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "may") {
      Result = StackLifetime::LivenessType::May;
    } else if (ParamName == "must") {
      Result = StackLifetime::LivenessType::Must;
    } else {
      assert(false &&
             formatv("invalid StackLifetime parameter '{0}' ", ParamName)
                 .str()
                 .c_str());
    }
  }
  return Result;
}

GVNOptions parseGVNOptions(StringRef Params) {
  GVNOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "pre") {
      Result.setPRE(Enable);
    } else if (ParamName == "load-pre") {
      Result.setLoadPRE(Enable);
    } else if (ParamName == "split-backedge-load-pre") {
      Result.setLoadPRESplitBackedge(Enable);
    } else if (ParamName == "memdep") {
      Result.setMemDep(Enable);
    } else {
      assert(false && formatv("invalid GVN pass parameter '{0}' ", ParamName)
                          .str()
                          .c_str());
    }
  }
  return Result;
}
bool parseMergedLoadStoreMotionOptions(StringRef Params) {
  bool Result = false;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "split-footer-bb") {
      Result = Enable;
    } else {
      assert(false &&
             formatv("invalid MergedLoadStoreMotion pass parameter '{0}' ",
                     ParamName)
                 .str()
                 .c_str());
    }
  }
  return Result;
}

bool checkParametrizedPassName(StringRef Name, StringRef PassName) {
   if (!Name.consume_front(PassName))
     return false;
   // normal pass name w/o parameters == default parameters
   if (Name.empty())
     return true;
   return Name.startswith("<") && Name.endswith(">");
 }

  AddressSanitizerOptions parseASanPassOptions(StringRef Params) {
   AddressSanitizerOptions Result;
   while (!Params.empty()) {
     StringRef ParamName;
     std::tie(ParamName, Params) = Params.split(';');
  
     if (ParamName == "kernel") {
       Result.CompileKernel = true;
     } else {
       assert(false&&formatv("invalid AddressSanitizer pass parameter '{0}' ", ParamName)
               .str().c_str());
     }
   }
   return Result;
 }

  HWAddressSanitizerOptions parseHWASanPassOptions(StringRef Params) {
   HWAddressSanitizerOptions Result;
   while (!Params.empty()) {
     StringRef ParamName;
     std::tie(ParamName, Params) = Params.split(';');
  
     if (ParamName == "recover") {
       Result.Recover = true;
     } else if (ParamName == "kernel") {
       Result.CompileKernel = true;
     } else {
       assert(false&&formatv("invalid HWAddressSanitizer pass parameter '{0}' ", ParamName)
               .str().c_str());
     }
   }
   return Result;
 }
  
} */ // namespace

LLVMOptimizer::LLVMOptimizer(std::string optArgs) {
  this->optArgs = optArgs;
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  if(auto E=PB.parsePassPipeline(FPM,optArgs)){
    llvm::errs()<<E<<"\n";
    llvm::errs()<<"Cannot parse "<<optArgs<<" passes\n";
  }
  if(auto E=PB.parsePassPipeline(MPM,optArgs)){
    llvm::errs()<<E<<"\n";
    llvm::errs()<<"Cannot parse "<<optArgs<<" passes\n";
  }

  /*if (optArgs == "O2") {
    FPM = PB.buildFunctionSimplificationPipeline(
        llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
    MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
  } else {
    std::string tmpArgs = optArgs;
    StringRef argsRef(tmpArgs);
    while (!argsRef.empty()) {
      StringRef Name;
      std::tie(Name, argsRef) = argsRef.split(',');
#define MODULE_PASS(NAME, CREATE_PASS)                                         \
  if (Name == "instrprof") {                                                   \
    MPM.addPass(InstrProfiling(InstrProfOptions()));                           \
  } else if (Name == NAME) {                                                   \
    MPM.addPass(CREATE_PASS);                                                  \
  }
 #define MODULE_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)      \
   if (checkParametrizedPassName(Name, NAME)) {                                 \
     MPM.addPass(CREATE_PASS(PARSER(PARAMS)));                                    \
   }
    #define CGSCC_PASS(NAME, CREATE_PASS)                                          \
   if (Name == NAME) {                                                          \
     MPM.addPass(createModuleToPostOrderCGSCCPassAdaptor(CREATE_PASS));         \
   }
   #define CGSCC_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)       \
   if (checkParametrizedPassName(Name, NAME)) {                                 \
     MPM.addPass(                                                               \
         createModuleToPostOrderCGSCCPassAdaptor(CREATE_PASS(PARSER(PARAMS))));   \
   }
#define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    MPM.addPass(createModuleToFunctionPassAdaptor(CREATE_PASS));               \
    FPM.addPass(CREATE_PASS);                                                  \
  }
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  if (checkParametrizedPassName(Name, NAME)) {                                                          \
    MPM.addPass(                                                               \
        createModuleToFunctionPassAdaptor(CREATE_PASS(PARSER(PARAMS))));       \
    FPM.addPass(CREATE_PASS(PARSER(PARAMS)));                                  \
  }
 #define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
   if (Name == NAME) {                                                          \
     MPM.addPass(createModuleToFunctionPassAdaptor(                             \
         createFunctionToLoopPassAdaptor(CREATE_PASS)));          \
     FPM.addPass(createFunctionToLoopPassAdaptor(CREATE_PASS));  \
   }
 #define LOOP_PASS(NAME, CREATE_PASS)                                           \
   if (Name == NAME) {                                                          \
     MPM.addPass(createModuleToFunctionPassAdaptor(                             \
         createFunctionToLoopPassAdaptor(CREATE_PASS)));          \
     FPM.addPass(createFunctionToLoopPassAdaptor(CREATE_PASS));  \
   }
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  if (checkParametrizedPassName(Name, NAME)) {                                                          \
    MPM.addPass(createModuleToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS(PARSER(PARAMS)))));        \
    FPM.addPass(createFunctionToLoopPassAdaptor(CREATE_PASS(PARSER(PARAMS)))); \
  }
 
#include "lib/Passes/PassRegistry.def"
    }
  }*/
}

llvm::Module *LLVMOptimizer::optimizeModule(llvm::Module *M) {
  MPM.run(*M, MAM);
  return M;
}

llvm::Function *LLVMOptimizer::optimizeFunction(llvm::Function *func) {
  FPM.run(*func, FAM);
  return func;
}
