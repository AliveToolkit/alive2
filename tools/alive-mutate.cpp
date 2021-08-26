// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"
#include "tools/mutator-utils/SingleLineMutator.h"

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
#include "llvm/Support/CommandLine.h"

using namespace tools;
using namespace util;
using namespace std;


#define LLVM_ARGS_PREFIX ""
#define ARGS_SRC_TGT
#define ARGS_REFINEMENT


namespace {

  llvm::cl::opt<string> testfile(llvm::cl::Positional,
    llvm::cl::desc("input test file"),
    llvm::cl::Required, llvm::cl::value_desc("filename"));

  llvm::cl::opt<string> outputFolder(llvm::cl::Positional,
    llvm::cl::desc("output file folder"),
    llvm::cl::Required, llvm::cl::value_desc("folder"));

  llvm::cl::opt<int> numCopy("n",llvm::cl::desc("number copies of test files"),llvm::cl::init(-1));

  llvm::cl::opt<int> timeElapsed("t",llvm::cl::desc("seconds of mutator should run"),llvm::cl::init(-1));


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

  vector<string> getOutputFiles(const string& inputFile,const string& outputPath,int numCopy){
    vector<string> res;
    string opt=(outputPath.back()=='/'?outputPath:(outputPath+'/'));
    string inputFileName=Util::split(inputFile,"/").back();
    if(size_t pos=inputFileName.find_last_of('.');pos!=string::npos){
        string fileName=string(inputFileName.begin(),inputFileName.begin()+pos);
        string extension=inputFileName.substr(pos+1);
        for(int i=0;i<numCopy;++i){
            res.push_back(opt+fileName+std::to_string(i)+"."+extension);
        }
    }
    return res;
  }
  /*llvm::Function *findFunction(llvm::Module &M, const string &FName) {
    for (auto &F : M) {
      if (F.isDeclaration())
        continue;
      if (FName.compare(F.getName()) != 0)
        continue;
      return &F;
    }
    return 0;
  }*/
}

void copyMode(),timeMode();

int main(int argc, char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  llvm::LLVMContext Context;

  std::string Usage =
      R"EOF(Alive2 stand-alone llvm test mutator )EOF";
  Usage += alive_version;
  Usage += R"EOF(empty for now)EOF";

  //llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);
  if(numCopy<0&&timeElapsed<0){
    cerr<<"Please specify either number of copies or running time!\n";
    return -1;
  }

  cout<<testfile<<' '<<numCopy<<' '<<timeElapsed<<"\n";
  
  if(numCopy>0){
    copyMode();
  }else if(timeElapsed>0){
    timeMode();
  }

  auto M1 = openInputFile(Context, testfile);
  if (!M1.get()) {
    cerr << "Could not read bitcode from '" << testfile << "'\n";
    return -1;
  }

  for(const llvm::Function& func:*M1){
    func.print(llvm::outs());
  }
  return 0;
}

void copyMode(){
  SingleLineMutator mutator;
  if(mutator.openInputFile(testfile)&&mutator.init()){
    
  }
}

void timeMode(){

}
