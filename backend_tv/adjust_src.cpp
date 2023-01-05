// include first to avoid ambiguity for comparison operator from
// util/spaceship.h
#include "llvm/MC/MCAsmInfo.h"

#include "backend_tv/backend_tv.h"
#include "util/sort.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;
using namespace llvm;
using namespace lifter;

namespace {

// FIXME -- each of adjustSrcInputs and adjustSrcReturn creates an
// entirely new function, this is slow and not elegant, probably merge
// these together

Function *adjustSrcInputs(Function *srcFn) {
  vector<Type *> new_argtypes;
  unsigned idx = 0;

  for (auto &v : srcFn->args()) {
    auto *ty = v.getType();    
    if (!ty->isIntegerTy()) // FIXME
      report_fatal_error("[Unsupported Function Argument]: Only int types "
                         "supported for now");
    auto orig_width = ty->getIntegerBitWidth();
    if (orig_width > 64) // FIXME
      report_fatal_error("[Unsupported Function Argument]: Only int types 64 "
                         "bits or smaller supported for now");
    if (orig_width < 64)
      new_input_idx_bitwidth.emplace_back(idx, orig_width);
    new_argtypes.emplace_back(Type::getIntNTy(srcFn->getContext(), 64));
    // FIXME Do we need to update the value_cache?
    idx++;
  }

  FunctionType *NFTy = FunctionType::get(srcFn->getReturnType(), new_argtypes, false);
  Function *NF = Function::Create(NFTy, srcFn->getLinkage(), srcFn->getAddressSpace(),
                                  srcFn->getName(), srcFn->getParent());
  NF->copyAttributesFrom(srcFn);
  // FIXME -- copy over argument attributes
  NF->splice(NF->begin(), srcFn);
  NF->takeName(srcFn);
  for (Function::arg_iterator I = srcFn->arg_begin(), E = srcFn->arg_end(),
         I2 = NF->arg_begin();
       I != E; ++I, ++I2) {
    if (I->getType()->getIntegerBitWidth() < 64) {
      auto name = I->getName().substr(I->getName().rfind('%')) + "_t";
      auto trunc = new TruncInst(I2,
                                 I->getType(),
                                 name,
                                 NF->getEntryBlock().getFirstNonPHI());
      I->replaceAllUsesWith(trunc);      
    } else {
      I->replaceAllUsesWith(&*I2);
    }
  }

  // FIXME -- doesn't matter if we're just dealing with one function,
  // but if we're lifting modules with important calls, we need to
  // replace uses of the function with NF, see code in
  // DeadArgumentElimination.cpp
  
  srcFn->eraseFromParent();
  return NF;
}

Function *adjustSrcReturn(Function *srcFn) {
  // FIXME -- there's some work to be done here for checking zeroext
  
  if (!srcFn->hasRetAttribute(Attribute::SExt))
    return srcFn;

  auto *ret_typ = srcFn->getReturnType();
  orig_ret_bitwidth = ret_typ->getIntegerBitWidth();
  cout << "original return bitwidth = " << orig_ret_bitwidth << endl;

  // FIXME
  if (!ret_typ->isIntegerTy())
    report_fatal_error("[Unsupported Function Return]: Only int types "
                       "supported for now");

  // FIXME
  if (orig_ret_bitwidth > 64)
    report_fatal_error("[Unsupported Function Return]: Only int types 64 "
                       "bits or smaller supported for now");

  // don't need to do any extension if the return type is exactly 32 bits
  if (orig_ret_bitwidth == 64 || orig_ret_bitwidth == 32)
    return srcFn;

  // starting here we commit to returning a copy instead of the
  // original function
  
  has_ret_attr = true;
  auto *i32ty = Type::getIntNTy(srcFn->getContext(), 32);
  auto *i64ty = Type::getIntNTy(srcFn->getContext(), 64);

  // build this first to avoid iterator invalidation
  vector<ReturnInst *> RIs;  
  for (auto &BB : *srcFn)
    for (auto &I : BB)
      if (auto *RI = dyn_cast<ReturnInst>(&I))
        RIs.push_back(RI);

  for (auto *RI : RIs) {
    auto retVal = RI->getReturnValue();
    if (orig_ret_bitwidth < 32) {
      auto sext = new SExtInst(retVal, i32ty,
                               retVal->getName() + "_sext",
                               RI);
      auto zext = new ZExtInst(sext, i64ty,
                               retVal->getName() + "_zext",
                               RI);
      ReturnInst::Create(srcFn->getContext(),
                         zext, RI);
    } else {
      auto sext = new SExtInst(retVal, i64ty,
                               retVal->getName() + "_sext",
                               RI);
      ReturnInst::Create(srcFn->getContext(),
                         sext, RI);
    }
    RI->eraseFromParent();
  }

  // FIXME this is duplicate code, factor it out
  FunctionType *NFTy = FunctionType::get(i64ty,
                                        srcFn->getFunctionType()->params(), false);
  Function *NF = Function::Create(NFTy, srcFn->getLinkage(), srcFn->getAddressSpace(),
                                  srcFn->getName(), srcFn->getParent());
  NF->copyAttributesFrom(srcFn);
  // FIXME -- copy over argument attributes
  NF->splice(NF->begin(), srcFn);
  NF->takeName(srcFn);
  for (Function::arg_iterator I = srcFn->arg_begin(), E = srcFn->arg_end(),
         I2 = NF->arg_begin();
       I != E; ++I, ++I2)
    I->replaceAllUsesWith(&*I2);

  // FIXME -- if we're lifting modules with important calls, we need to replace
  // uses of the function with NF, see code in DeadArgumentElimination.cpp
  
  srcFn->eraseFromParent();
  return NF;
}

} // namespace

namespace lifter {

Function *adjustSrc(Function *srcFn) {
  if (srcFn->isVarArg())
    report_fatal_error("Varargs not supported");

  outs() << "\n---------- src.ll ----------\n";
  srcFn->print(outs());

  srcFn = adjustSrcInputs(srcFn);
  
  outs() << "\n---------- src.ll ---- changed-input -\n";
  srcFn->print(outs());

  srcFn = adjustSrcReturn(srcFn);
  
  outs() << "\n---------- src.ll ---- changed-return -\n";
  srcFn->print(outs());

  return srcFn;
}

} // namespace
