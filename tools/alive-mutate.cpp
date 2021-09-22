// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.



#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"
#include "tools/mutator-utils/simpleMutator.h"
#include "tools/mutator-utils/ComplexMutator.h"

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
#include "llvm/IR/Verifier.h"

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


  llvm::cl::opt<string> testfile(llvm::cl::Positional,
    llvm::cl::desc("inputTestFile"),
    llvm::cl::Required, llvm::cl::value_desc("filename"));

  llvm::cl::opt<string> outputFolder(llvm::cl::Positional,
    llvm::cl::desc("outputFileFolder"),
    llvm::cl::Required, llvm::cl::value_desc("folder"));

  llvm::cl::opt<int> numCopy("n",llvm::cl::desc("number copies of test files"),llvm::cl::init(-1));

  llvm::cl::opt<int> timeElapsed("t",llvm::cl::desc("seconds of mutator should run"),llvm::cl::init(-1));
  llvm::cl::opt<bool> verbose("v",llvm::cl::desc("verbose mode"));

  filesystem::path inputPath,outputPath;

optional<smt::smt_initializer> smt_init;

struct Results {
  Transform t;
  string error;
  Errors errs;
  enum {
    ERROR,
    TYPE_CHECKER_FAILED,
    SYNTACTIC_EQ,
    CORRECT,
    UNSOUND,
    FAILED_TO_PROVE
  } status;

  static Results Error(string &&err) {
    Results r;
    r.status = ERROR;
    r.error = move(err);
    return r;
  }
};

Results verify(llvm::Function &F1, llvm::Function &F2,
               llvm::TargetLibraryInfoWrapperPass &TLI,
               bool print_transform = false,
               bool always_verify = false) {
  auto fn1 = llvm2alive(F1, TLI.getTLI(F1));
  if (!fn1)
    return Results::Error("Could not translate '" + F1.getName().str() +
                          "' to Alive IR\n");

  auto fn2 = llvm2alive(F2, TLI.getTLI(F2), fn1->getGlobalVarNames());
  if (!fn2)
    return Results::Error("Could not translate '" + F2.getName().str() +
                          "' to Alive IR\n");

  Results r;
  r.t.src = move(*fn1);
  r.t.tgt = move(*fn2);

  if (!always_verify) {
    stringstream ss1, ss2;
    r.t.src.print(ss1);
    r.t.tgt.print(ss2);
    if (ss1.str() == ss2.str()) {
      if (print_transform)
        r.t.print(*out, {});
      r.status = Results::SYNTACTIC_EQ;
      return r;
    }
  }

  smt_init->reset();
  r.t.preprocess();
  TransformVerify verifier(r.t, false);

  if (print_transform)
    r.t.print(*out, {});

  {
    auto types = verifier.getTypings();
    if (!types) {
      r.status = Results::TYPE_CHECKER_FAILED;
      return r;
    }
    assert(types.hasSingleTyping());
  }

  r.errs = verifier.verify();
  if (r.errs) {
    r.status = r.errs.isUnsound() ? Results::UNSOUND : Results::FAILED_TO_PROVE;
  } else {
    r.status = Results::CORRECT;
  }
  return r;
}

unsigned num_correct = 0;
unsigned num_unsound = 0;
unsigned num_failed = 0;
unsigned num_errors = 0;

bool compareFunctions(llvm::Function &F1, llvm::Function &F2,
                      llvm::TargetLibraryInfoWrapperPass &TLI) {
  auto r = verify(F1, F2, TLI, !opt_quiet, opt_always_verify);
  if (r.status == Results::ERROR) {
    *out << "ERROR: " << r.error;
    ++num_errors;
    return true;
  }
  switch (r.status) {
  case Results::ERROR:
    UNREACHABLE();
    break;

  case Results::SYNTACTIC_EQ:
    *out << "Transformation seems to be correct! (syntactically equal)\n\n";
    ++num_correct;
    break;

  case Results::CORRECT:
    *out << "Transformation seems to be correct!\n\n";
    ++num_correct;
    break;

  case Results::TYPE_CHECKER_FAILED:
    *out << "Transformation doesn't verify!\n"
            "ERROR: program doesn't type check!\n\n";
    ++num_errors;
    return true;

  case Results::UNSOUND:
    *out << "Transformation doesn't verify!\n\n";
    if (!opt_quiet)
      *out << r.errs << endl;
    ++num_unsound;
    return false;

  case Results::FAILED_TO_PROVE:
    *out << r.errs << endl;
    ++num_failed;
    return true;
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

  llvm::FunctionPassManager FPM = PB.buildFunctionSimplificationPipeline(
      llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  llvm::ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.run(*M, MAM);
}
}

int logIndex;
void copyMode(),timeMode(),loggerInit(llvm::Module* pm),init(),runOnce(int ith,llvm::LLVMContext& context,Mutator& mutator);
bool isValidInputPath(),isValidOutputPath();
string getOutputFile(int ith,bool isOptimized=false);


int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  

  std::string Usage =
      R"EOF(Alive2 stand-alone LLVM test mutator:
version )EOF";
  Usage += alive_version;

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  if(numCopy<0&&timeElapsed<0){
    cerr<<"Please specify either number of copies or running time!\n";
    return -1;
  }else if(!isValidInputPath()){
    cerr<<"Input file does not exist!\n";
    return -1;
  }else if(!isValidOutputPath()){
    cerr<<"Output folder does not exist!\n";
    return -1;
  }
  init();
  if(numCopy>0){
    copyMode();
  }else if(timeElapsed>0){
    timeMode();
  }
  std::cout<<"program ended\n";
  return num_errors > 0;
}


/*
 * Adapted from llvm_util/cmd_args_def.h
 * Init part is moved here, and part of setting log is moved to loggerInit;
*/
void init(){
  config::src_unroll_cnt = opt_src_unrolling_factor;
  config::tgt_unroll_cnt = opt_tgt_unrolling_factor;
  config::disable_undef_input = opt_disable_undef;
  config::disable_poison_input = opt_disable_poison;
  config::symexec_print_each_value = opt_se_verbose;
  smt::set_query_timeout(to_string(opt_smt_to));
  smt::set_memory_limit((uint64_t)opt_smt_max_mem * 1024 * 1024);
  smt::set_random_seed(to_string(opt_smt_random_seed));
  config::skip_smt = opt_smt_skip;
  config::smt_benchmark_dir = opt_smt_bench_dir;
  smt::solver_print_queries(opt_smt_verbose);
  smt::solver_tactic_verbose(opt_tactic_verbose);
  config::debug = opt_debug;
  config::max_offset_bits = opt_max_offset_in_bits;

  func_names.insert(opt_funcs.begin(), opt_funcs.end());
}

/*
 * Set Alive2's log path. if verbose flag is used, it could output to /def/null or stdout. 
 * Otherwise it will output to file if find a value mismatch
*/
void loggerInit(llvm::Module* pm){
  static std::ofstream nout("/dev/null");
  if(verbose){
      out=&nout;
      //out=&cout;
  }else{
      auto &source_file = pm->getSourceFileName();
      fs::path fname = "log"+to_string(logIndex)+".txt";
      fs::path path = fs::path(outputFolder.getValue()) / fname.filename();
      if(out_file.is_open())out_file.close();
      out_file.open(path);
      out = &out_file;
      if (!out_file.is_open()) {
        cerr << "Alive2: Couldn't open report file!" << endl;
        exit(1);
      }

      report_filename = path;
      *out << "Source: " << source_file << endl;
      report_dir_created = true;

      if (opt_smt_log) {
        fs::path path_z3log = path;
        path_z3log.replace_extension("z3_log.txt");
        smt::start_logging(path_z3log.c_str());
      }
  }
  util::config::set_debug(*out);
}

bool isValidInputPath(){
  bool result=filesystem::status(string(testfile)).type()==filesystem::file_type::regular;
  if(result){
    inputPath=filesystem::path(string(testfile));
  }
  return result;
}

bool isValidOutputPath(){
  bool result= filesystem::status(string(outputFolder)).type()==filesystem::file_type::directory;
  if(result){
    outputPath=filesystem::path(string(outputFolder));
  }
  return result;
}

string getOutputFile(int ith,bool isOptimized){
  static string templateName=string(outputFolder)+inputPath.stem().string();
  return templateName+to_string(ith)+(isOptimized?"-opt.ll":".ll");
}


/*
 * Mutate file once and send it and its optmized version into Alive2
 * LogIndex is updated here if find a value mismatch.
*/
void runOnce(int ith,llvm::LLVMContext& context,Mutator& mutator){
    std::unique_ptr<llvm::Module> M1=nullptr;
<<<<<<< HEAD
    mutator.mutateModule(getOutputFile(ith));
    M1 = mutator.getModule();
=======
    bool isSimpleMutate=false;//Random::getRandomBool();
    if(false){
      mutator.generateTest(getOutputFile(verbose?ith:-1));
      M1 = openInputFile(context, getOutputFile(verbose?ith:-1));
    }else{
      cmutator.generateTest(getOutputFile(ith));
      M1=cmutator.getModule();
    }
>>>>>>> only verifying mutated function now
    
    if (!M1.get()) {
      cerr << "Could not read file from '" << getOutputFile(ith)<< "'\n";
      return;
    }
    loggerInit(M1.get());

    auto &DL = M1.get()->getDataLayout();
    llvm::Triple targetTriple(M1.get()->getTargetTriple());
    llvm::TargetLibraryInfoWrapperPass TLI(targetTriple);

    llvm_util::initializer llvm_util_init(*out, DL);
    smt_init.emplace();

    unique_ptr<llvm::Module> M2;
    M2 = CloneModule(*M1);
    optimizeModule(M2.get());

<<<<<<< HEAD
    const string optFunc=mutator.getCurrentFunction();
    if(llvm::Function* pf1=M1->getFunction(optFunc);pf1!=nullptr){
=======
    const string optFunc=cmutator.getCurrentFunction();
    if(llvm::Function* pf1=M1->getFunction(optFunc);pf1!=nullptr){
    //for(llvm::Function& f1:*M1){
>>>>>>> only verifying mutated function now
      if(!pf1->isDeclaration()){
        if(llvm::Function* pf2=M2->getFunction(optFunc);pf2!=nullptr){
            if (!compareFunctions(*pf1, *pf2, TLI))
              if (opt_error_fatal)
                goto end;
        }
      }
    }
    if(num_unsound>0){
      ++logIndex;
      std::cout<<"Unsound found! at "<<ith<<"th copies\n";
    }
    *out << "Summary:\n"
            "  " << num_correct << " correct transformations\n"
            "  " << num_unsound << " incorrect transformations\n"
            "  " << num_failed  << " failed-to-prove transformations\n"
            "  " << num_errors << " Alive2 errors\n";

  end:
    if (opt_smt_stats)
      smt::solver_print_stats(*out);
    smt_init.reset();
    num_correct=num_unsound=num_failed=num_errors=0;
    mutator.setModule(std::move(M1));
}

/*
 * call runOnce for numCopy times.
*/
void copyMode(){
  llvm::LLVMContext context;
  std::unique_ptr<Mutator> mutators[2]{std::make_unique<SimpleMutator>(verbose),std::make_unique<ComplexMutator>(verbose)};
  if(mutators[0]->openInputFile(testfile)&&mutators[1]->openInputFile(testfile)){
    if(bool sInit=mutators[0]->init(),cInit=mutators[1]->init();sInit||cInit){
      for(int i=0;i<numCopy;++i){
        if(true){
          std::cout<<"Running "<<i<<"th copies."<<std::endl;
        }
        if(sInit^cInit){
          runOnce(i,context,*mutators[sInit?0:1]);
        }else{
          runOnce(i,context,*mutators[Random::getRandomUnsigned()&1]);
        }
      }
    }else{
      cerr<<"Cannot find any locations to mutate, "+testfile+" skipped\n!";
      return;
    }
  }
}

/*
 * keep calling runOnce and soft exit once time's up.
*/
void timeMode(){
  llvm::LLVMContext context;
  std::unique_ptr<Mutator> mutators[2]{std::make_unique<SimpleMutator>(verbose),std::make_unique<ComplexMutator>(verbose)};
  if(mutators[0]->openInputFile(testfile)&&mutators[1]->openInputFile(testfile)){
    bool sInit=mutators[0]->init();
    bool cInit=mutators[1]->init();
    if(!sInit&&!cInit){
      cerr<<"Cannot find any lotaion to mutate, "+testfile+" skipped\n";
      return;
    }
    std::chrono::duration<double> sum=std::chrono::duration<double>::zero();
    int cnt=1;
    while(sum.count()<timeElapsed){
      auto t_start = std::chrono::high_resolution_clock::now();
      if(sInit^cInit){
        runOnce(cnt,context,*mutators[sInit?0:1]);
      }else{
        runOnce(cnt,context,*mutators[Random::getRandomUnsigned()&1]);
      }
      auto t_end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> cur=t_end-t_start;
      if(true){
        std::cout<<"Generted "+to_string(cnt)+"th copies in "+to_string((cur).count())+" seconds\n";
      }
      sum+=cur;
      ++cnt;
    }
  }else{
    cerr<<"Cannot opne your input file "+testfile+"!\n";
  }
}