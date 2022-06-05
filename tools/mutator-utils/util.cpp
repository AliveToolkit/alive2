#include "util.h"

std::random_device Random::rd;
std::uniform_int_distribution<int> Random::dist(0, 2147483647u);
unsigned Random::seed(rd());
std::mt19937 Random::mt(Random::seed);

llvm::SmallVector<unsigned> Random::usedInts;
llvm::SmallVector<double> Random::usedDoubles;
llvm::SmallVector<float> Random::usedFloats;
LLVMFunctionComparator LLVMUtil::comparator;

unsigned Random::getExtremeInt(llvm::IntegerType *ty) {
  unsigned size = ty->getBitWidth();
  size = std::min(size, 32u);
  if (size <= 7) {
    return getRandomUnsigned(size);
  } else {
    return getRandomBool()
               ? (0 + getRandomUnsigned(7))
               : (unsigned)((1ull << size) - getRandomUnsigned(7) - 1);
  }
}

unsigned Random::getBitmask(llvm::IntegerType *ty) {
  unsigned size = ty->getBitWidth();
  size = std::min(size, 32u);
  unsigned result = (unsigned)((1ull << size) - 1);
  unsigned le = (unsigned)((1ull << (1 + getRandomUnsigned() % 32)) - 1);
  unsigned ri = (unsigned)((1ull << (getRandomUnsigned() % le)) - 1);
  return result ^ le ^ ri;
}

double Random::getExtremeDouble() {
  return 0;
}

float Random::getExtremeFloat() {
  return 0;
}

double Random::getRandomDouble() {
  return 0;
}

float Random::getRandomFloat() {
  return 0;
}

unsigned Random::getUsedInt(llvm::IntegerType *ty) {
  if (usedInts.empty()) {
    return getRandomUnsigned(ty->getBitWidth());
  } else {
    for (size_t i = 0, pos = getRandomUnsigned() % usedInts.size();
         i < usedInts.size(); ++i, ++pos) {
      if (pos == usedInts.size()) {
        pos = 0;
      }
      if (usedInts[pos] <= ty->getBitMask()) {
        return usedInts[pos];
      }
    }
    return getRandomUnsigned(ty->getBitWidth());
  }
}

double Random::getUsedDouble() {
  return 0;
}

float Random::getUsedFloat() {
  return 0;
}

unsigned Random::getRandomLLVMInt(llvm::IntegerType *ty) {
  switch (getRandomUnsigned(2)) {
  case 0:
    return getUsedInt(ty);
  case 1:
    return getBitmask(ty);
    ;
  case 2:
    return getExtremeInt(ty);
  case 3:
    return getRandomUnsigned(ty->getBitWidth());
  default:
    return getRandomUnsigned(ty->getBitWidth());
  }
}

double Random::getRandomLLVMDouble() {
  switch (getRandomUnsigned() % 3) {
  case 0:
    return getUsedDouble();
  case 1:
    return getExtremeDouble();
  case 2:
    return getRandomDouble();
  default:
    return getRandomDouble();
  }
}

float Random::getRandomLLVMFloat() {
  switch (getRandomUnsigned() % 3) {
  case 0:
    return getUsedFloat();
  case 1:
    return getExtremeFloat();
  case 2:
    return getRandomFloat();
  default:
    return getRandomFloat();
  }
}

void LLVMUtil::optimizeModule(llvm::Module *M, bool newGVN, bool licm) {
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

  llvm::FunctionPassManager FPM;
  if (newGVN || licm) {
    if (newGVN) {
      FPM.addPass(llvm::NewGVNPass());
    }
    if (licm) {
      FPM.addPass(llvm::createFunctionToLoopPassAdaptor(
          llvm::LICMPass(llvm::LICMOptions()), true));
    }
  } else {
    FPM = PB.buildFunctionSimplificationPipeline(
        llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  }
  llvm::ModulePassManager MPM;
  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
  MPM.run(*M, MAM);
}

void LLVMUtil::optimizeFunction(llvm::Function *f, bool newGVN, bool licm) {
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

  llvm::FunctionPassManager FPM;
  if (newGVN || licm) {
    if (newGVN) {
      FPM.addPass(llvm::NewGVNPass());
    }
    if (licm) {
      FPM.addPass(llvm::createFunctionToLoopPassAdaptor(
          llvm::LICMPass(llvm::LICMOptions()), true));
    }
  } else {
    FPM = PB.buildFunctionSimplificationPipeline(
        llvm::OptimizationLevel::O2, llvm::ThinOrFullLTOPhase::None);
  }
  FPM.run(*f, FAM);
}

llvm::Value *LLVMUtil::insertGlobalVariable(llvm::Module *m, llvm::Type *ty) {
  static const std::string GLOBAL_VAR_NAME_PREFIX = "aliveMutateGlobalVar";
  static int varCount = 0;
  m->getOrInsertGlobal(GLOBAL_VAR_NAME_PREFIX + std::to_string(varCount), ty);
  llvm::GlobalVariable *val =
      m->getGlobalVariable(GLOBAL_VAR_NAME_PREFIX + std::to_string(varCount));
  ++varCount;
  val->setLinkage(llvm::GlobalValue::LinkageTypes::ExternalLinkage);
  val->setAlignment(llvm::MaybeAlign(1));
  return val;
}

void LLVMUtil::insertFunctionArguments(llvm::Function *F,
                                       llvm::SmallVector<llvm::Type *> tys,
                                       llvm::ValueToValueMapTy &VMap) {
  // uptated from llvm CloneFunction
  using llvm::Argument;
  using llvm::Function;
  using llvm::FunctionType;
  using llvm::Type;
  std::vector<Type *> ArgTypes;

  // The user might be deleting arguments to the function by specifying them in
  // the VMap.  If so, we need to not add the arguments to the arg ty vector
  //
  for (const Argument &I : F->args())
    if (VMap.count(&I) == 0) // Haven't mapped the argument to anything yet?
      ArgTypes.push_back(I.getType());
  for (const auto &ty : tys)
    ArgTypes.push_back(ty);

  // Create a new function type...
  FunctionType *FTy =
      FunctionType::get(F->getFunctionType()->getReturnType(), ArgTypes,
                        F->getFunctionType()->isVarArg());

  // Create the new function...
  Function *NewF = Function::Create(FTy, F->getLinkage(), F->getAddressSpace(),
                                    F->getName(), F->getParent());

  // Loop over the arguments, copying the names of the mapped arguments over...
  Function::arg_iterator DestI = NewF->arg_begin();
  for (const Argument &I : F->args())
    if (VMap.count(&I) == 0) {     // Is this argument preserved?
      DestI->setName(I.getName()); // Copy the name over...
      VMap[&I] = &*DestI++;        // Add mapping to VMap
    }

  llvm::SmallVector<llvm::ReturnInst *, 8> Returns; // Ignore returns cloned.
  CloneFunctionInto(NewF, F, VMap,
                    llvm::CloneFunctionChangeType::LocalChangesOnly, Returns,
                    "", nullptr);
  std::string oldFuncName = F->getName().str(),
              newFuncName = NewF->getName().str();
  NewF->setName("tmpFunctionNameQuinella");
  F->setName(newFuncName);
  NewF->setName(oldFuncName);
}

void LLVMUtil::removeTBAAMetadata(llvm::Module *M) {
  for (auto fit = M->begin(); fit != M->end(); fit++) {
    if (!fit->isDeclaration() && !fit->getName().empty()) {
      for (auto iit = llvm::inst_begin(*fit); iit != llvm::inst_end(*fit);
           iit++) {
        iit->setMetadata("tbaa", nullptr);
      }
    }
  }
}

const std::vector<llvm::Instruction::BinaryOps> LLVMUtil::integerBinaryOps{
    llvm::Instruction::Add,  llvm::Instruction::Sub,  llvm::Instruction::Mul,
    llvm::Instruction::SDiv, llvm::Instruction::UDiv, llvm::Instruction::SRem,
    llvm::Instruction::URem, llvm::Instruction::Shl,  llvm::Instruction::LShr,
    llvm::Instruction::AShr, llvm::Instruction::And,  llvm::Instruction::Xor,
    llvm::Instruction::Or};

const std::vector<llvm::Instruction::BinaryOps> LLVMUtil::floatBinaryOps{
    llvm::Instruction::FAdd, llvm::Instruction::FSub, llvm::Instruction::FMul,
    llvm::Instruction::FDiv, llvm::Instruction::FRem};

const std::vector<llvm::Intrinsic::ID> LLVMUtil::integerBinaryIntrinsic{
    llvm::Intrinsic::smax,
    llvm::Intrinsic::smin,
    llvm::Intrinsic::umax,
    llvm::Intrinsic::umin,
    llvm::Intrinsic::sadd_sat,
    llvm::Intrinsic::uadd_sat,
    llvm::Intrinsic::ssub_sat,
    llvm::Intrinsic::usub_sat,
    llvm::Intrinsic::sshl_sat,
    llvm::Intrinsic::ushl_sat

};

const std::vector<llvm::Intrinsic::ID> LLVMUtil::floatBinaryIntrinsic{
    llvm::Intrinsic::pow,     llvm::Intrinsic::minnum,
    llvm::Intrinsic::maxnum,  llvm::Intrinsic::minimum,
    llvm::Intrinsic::maximum, llvm::Intrinsic::copysign};

const std::vector<llvm::Intrinsic::ID> LLVMUtil::integerUnaryIntrinsic{
    llvm::Intrinsic::bitreverse,
    llvm::Intrinsic::bswap,
    llvm::Intrinsic::ctpop,
};

const std::vector<llvm::Intrinsic::ID> LLVMUtil::floatUnaryIntrinsic{
    llvm::Intrinsic::sqrt,        llvm::Intrinsic::sin,
    llvm::Intrinsic::cos,         llvm::Intrinsic::exp,
    llvm::Intrinsic::exp2,        llvm::Intrinsic::log,
    llvm::Intrinsic::log10,       llvm::Intrinsic::log2,
    llvm::Intrinsic::fabs,        llvm::Intrinsic::floor,
    llvm::Intrinsic::ceil,        llvm::Intrinsic::trunc,
    llvm::Intrinsic::rint,        llvm::Intrinsic::nearbyint,
    llvm::Intrinsic::round,       llvm::Intrinsic::roundeven,
    llvm::Intrinsic::canonicalize};

void LLVMUtil::insertRandomCodeBefore(llvm::Instruction *inst) {
  RandomCodePieceGenerator::insertCodeBefore(
      inst, 5 + (Random::getRandomUnsigned() & 15)); // last 4 binary bits
}

llvm::Instruction *
LLVMUtil::getRandomIntegerInstruction(llvm::Value *val1, llvm::Value *val2,
                                      llvm::Instruction *insertBefore) {
  assert(val1->getType()->isIntegerTy() &&
         "should be an integer to get an int instruction!");
  assert(val2->getType()->isIntegerTy() &&
         "should be an integer to get an int instruction!");
  return Random::getRandomBool()
             ? getRandomIntegerBinaryInstruction(val1, val2, insertBefore)
             : getRandomIntegerIntrinsic(val1, val2, insertBefore);
}

llvm::Instruction *
LLVMUtil::getRandomFloatInstruction(llvm::Value *val1, llvm::Value *val2,
                                    llvm::Instruction *insertBefore) {
  assert(val1->getType()->isFloatingPointTy() &&
         "should be a floating point to get a float instruction!");
  assert(val2->getType()->isFloatingPointTy() &&
         "should be a floating point to get a float instruction!");
  return Random::getRandomBool()
             ? getRandomFloatBinaryInstruction(val1, val2, insertBefore)
             : getRandomFloatInstrinsic(val1, val2, insertBefore);
}

llvm::Instruction *LLVMUtil::getRandomIntegerBinaryInstruction(
    llvm::Value *val1, llvm::Value *val2, llvm::Instruction *insertBefore) {
  assert(val1->getType()->isIntegerTy() &&
         "should be an integer to get an int instruction!");
  assert(val2->getType()->isIntegerTy() &&
         "should be an integer to get an int instruction!");
  llvm::Instruction::BinaryOps Op =
      integerBinaryOps[Random::getRandomUnsigned() % integerBinaryOps.size()];
  return llvm::BinaryOperator::Create(Op, val1, val2, "", insertBefore);
}

llvm::Instruction *
LLVMUtil::getRandomFloatBinaryInstruction(llvm::Value *val1, llvm::Value *val2,
                                          llvm::Instruction *insertBefore) {
  assert(val1->getType()->isFloatingPointTy() &&
         "should be a floating point to get a float instruction!");
  assert(val2->getType()->isFloatingPointTy() &&
         "should be a floating point to get a float instruction!");
  llvm::Instruction::BinaryOps Op =
      floatBinaryOps[Random::getRandomUnsigned() % floatBinaryOps.size()];
  return llvm::BinaryOperator::Create(Op, val1, val2, "", insertBefore);
}

llvm::Instruction *
LLVMUtil::getRandomIntegerIntrinsic(llvm::Value *val1, llvm::Value *val2,
                                    llvm::Instruction *insertBefore) {
  return nullptr;
}

llvm::Instruction *
LLVMUtil::getRandomFloatInstrinsic(llvm::Value *val1, llvm::Value *val2,
                                   llvm::Instruction *insertBefore) {
  return nullptr;
}