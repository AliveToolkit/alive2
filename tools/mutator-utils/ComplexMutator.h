#pragma once
#include <string>
#include <memory>
#include <vector>
#include "llvm/IR/Module.h"
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
#include "llvm/Support/Error.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Verifier.h"

class ComplexMutator{
    llvm::LLVMContext context;
    llvm::ExitOnError ExitOnErr;
    std::unique_ptr<llvm::Module> pm;

    std::vector<llvm::Value*> instArgs;
    std::vector<llvm::Instruction*> newAdded;
    llvm::Instruction* updatedInst;
    
    //static void optimizeModule(llvm::Module *M) ;
    bool debug;


    decltype(pm->begin()) fit;
    decltype(fit->begin()) bit;
    decltype(bit->begin()) iit;
    void moveToNextInst();
    void moveToNextBasicBlock();
    void moveToNextFuction();

    bool isReplaceable(llvm::Instruction* inst);
    void moveToNextReplaceableInst();
    void restoreBackUp();

public:
    ComplexMutator(bool debug=false):updatedInst(nullptr),debug(debug){};
    ~ComplexMutator(){};
    //bool openInputFile(const std::string& inputFile);// adapted from llvm-dis.cpp
    bool init();
    void generateTest(const std::string& outputFileName);
    void setDebug(bool debug){this->debug=debug;}
    //void generateOptimizedFile(const std::string& inputFile,const std::string& outputFile);
    bool openInputFile(const std::string &InputFilename);// adapted from llvm-dis.cpp
};

/*

class ComplexMutator{
    bool debug;
    static llvm::ExitOnError ExitOnErr;
    static std::unique_ptr<llvm::Module> pm;
    static llvm::LLVMContext context;
    
    decltype(pm->begin()) fit;
    decltype(fit->begin()) bit;
    decltype(bit->begin()) iit;
    void moveToNextInst();
    void moveToNextBasicBlock();
    void moveToNextFuction();

    bool isReplaceable(llvm::Instruction* inst);
    void moveToNextReplaceableInst();
    void restoreBackUp(llvm::Instruction* inst,std::vector<llvm::Value*> args,std::vector<llvm::Instruction*>& newAdded);

public:
    ComplexMutator(bool debug=false):debug(debug){};
    ~ComplexMutator(){};
    bool openInputFile(const std::string& inputFile);// adapted from llvm-dis.cpp
    bool init();
    void generateTest(const std::string& outputFileName);
    void setDebug(bool debug){this->debug=debug;}
    
};
*/