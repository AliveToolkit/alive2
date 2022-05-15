#include "LLVMOptimizer.h"

using namespace llvm;

 // The following passes/analyses have custom names, otherwise their name will
 // include `(anonymous namespace)`. These are special since they are only for
 // testing purposes and don't live in a header file.
  
 /// No-op module pass which does nothing.
 struct NoOpModulePass : PassInfoMixin<NoOpModulePass> {
   PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
     return PreservedAnalyses::all();
   }
  
   static StringRef name() { return "NoOpModulePass"; }
 };
  
 /// No-op module analysis.
 class NoOpModuleAnalysis : public AnalysisInfoMixin<NoOpModuleAnalysis> {
   friend AnalysisInfoMixin<NoOpModuleAnalysis>;
   static AnalysisKey Key;
  
 public:
   struct Result {};
   Result run(Module &, ModuleAnalysisManager &) { return Result(); }
   static StringRef name() { return "NoOpModuleAnalysis"; }
 };
  
 /// No-op CGSCC pass which does nothing.
 struct NoOpCGSCCPass : PassInfoMixin<NoOpCGSCCPass> {
   PreservedAnalyses run(LazyCallGraph::SCC &C, CGSCCAnalysisManager &,
                         LazyCallGraph &, CGSCCUpdateResult &UR) {
     return PreservedAnalyses::all();
   }
   static StringRef name() { return "NoOpCGSCCPass"; }
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
   static StringRef name() { return "NoOpCGSCCAnalysis"; }
 };
  
 /// No-op function pass which does nothing.
 struct NoOpFunctionPass : PassInfoMixin<NoOpFunctionPass> {
   PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
     return PreservedAnalyses::all();
   }
   static StringRef name() { return "NoOpFunctionPass"; }
 };
  
 /// No-op function analysis.
 class NoOpFunctionAnalysis : public AnalysisInfoMixin<NoOpFunctionAnalysis> {
   friend AnalysisInfoMixin<NoOpFunctionAnalysis>;
   static AnalysisKey Key;
  
 public:
   struct Result {};
   Result run(Function &, FunctionAnalysisManager &) { return Result(); }
   static StringRef name() { return "NoOpFunctionAnalysis"; }
 };
  
 /// No-op loop nest pass which does nothing.
 struct NoOpLoopNestPass : PassInfoMixin<NoOpLoopNestPass> {
   PreservedAnalyses run(LoopNest &L, LoopAnalysisManager &,
                         LoopStandardAnalysisResults &, LPMUpdater &) {
     return PreservedAnalyses::all();
   }
   static StringRef name() { return "NoOpLoopNestPass"; }
 };
  
 /// No-op loop pass which does nothing.
 struct NoOpLoopPass : PassInfoMixin<NoOpLoopPass> {
   PreservedAnalyses run(Loop &L, LoopAnalysisManager &,
                         LoopStandardAnalysisResults &, LPMUpdater &) {
     return PreservedAnalyses::all();
   }
   static StringRef name() { return "NoOpLoopPass"; }
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
   static StringRef name() { return "NoOpLoopAnalysis"; }
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
       if(ParamName.getAsInteger(0, Count)){
           assert(false&&"param get as integer failed");
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

LLVMOptimizer::LLVMOptimizer(std::string optArgs){
    this->optArgs = optArgs;
    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    if(optArgs=="O2"){
        FPM = PB.buildFunctionSimplificationPipeline(
        llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
        MPM=PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
    }else{
        using namespace llvm;
        std::string tmpArgs=optArgs;
        StringRef argsRef(tmpArgs);      
        while(!argsRef.empty()){
            StringRef Name;
            std::tie(Name, argsRef) = argsRef.split(',');
#define MODULE_PASS(NAME, CREATE_PASS)                                         \
    if (Name == NAME) {                                                          \
        MPM.addPass(CREATE_PASS);                                                  \
    }
 #define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
    if (Name == NAME) {                                                          \
        MPM.addPass(createModuleToFunctionPassAdaptor(CREATE_PASS));               \
        FPM.addPass(CREATE_PASS);                                                \
    }
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)                                           \
    if (Name == NAME) {                                                          \
        MPM.addPass(createModuleToFunctionPassAdaptor(                             \
         createFunctionToLoopPassAdaptor(CREATE_PASS(PARSER(PARAMS)))));          \
        FPM.addPass(createFunctionToLoopPassAdaptor(CREATE_PASS(PARSER(PARAMS))));            \
    }
#include "PassRegistry.def"
        }
    }
}

llvm::Module* LLVMOptimizer::optimizeModule(llvm::Module* M){
    MPM.run(*M,MAM);
    return M;
}

llvm::Function* LLVMOptimizer::optimizeFunction(llvm::Function* func){
    FPM.run(*func,FAM);
    return func;
}
