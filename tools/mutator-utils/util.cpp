#include "util.h"

std::random_device Random::rd;
std::uniform_int_distribution<int> Random::dist(0, 2147483647u);
unsigned Random::masterSeed(rd());
unsigned Random::seed(rd());
std::mt19937 Random::masterMt(Random::masterSeed);
std::mt19937 Random::mt(Random::seed);

llvm::SmallVector<unsigned> Random::usedInts;
llvm::SmallVector<double> Random::usedDoubles;
llvm::SmallVector<float> Random::usedFloats;
LLVMFunctionComparator mutator_util::comparator;

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
  return (double)getRandomUnsigned();
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

llvm::APInt mutator_util::getRandomLLVMInt(llvm::IntegerType *ty) {
  unsigned width = ty->getBitWidth();
  switch (Random::getRandomUnsigned(2)) {
  case 0:
    return llvm::APInt(width, Random::getUsedInt(ty));
  case 1:
    return llvm::APInt(width, Random::getBitmask(ty));
  case 2:
    return llvm::APInt(width, Random::getExtremeInt(ty));
  case 3:
    return llvm::APInt(width, Random::getRandomUnsigned(width));
  default:
    return llvm::APInt(width, Random::getRandomUnsigned(width));
  }
}

double mutator_util::getRandomLLVMDouble() {
  switch (Random::getRandomUnsigned() % 3) {
  case 0:
    return Random::getUsedDouble();
  case 1:
    return Random::getExtremeDouble();
  case 2:
    return Random::getRandomDouble();
  default:
    return Random::getRandomDouble();
  }
}

float mutator_util::getRandomLLVMFloat() {
  switch (Random::getRandomUnsigned() % 3) {
  case 0:
    return Random::getUsedFloat();
  case 1:
    return Random::getExtremeFloat();
  case 2:
    return Random::getRandomFloat();
  default:
    return Random::getRandomFloat();
  }
}

llvm::ConstantRange
mutator_util::getRandomLLVMConstantRange(llvm::IntegerType *ty) {
  unsigned width = ty->getBitWidth();
  unsigned num1 = Random::getRandomUnsigned(std::min(width, 32u)),
           num2 = Random::getRandomUnsigned(std::min(width, 32u));
  if (num1 == num2) {
    ++num2;
  }
  llvm::APInt lower(width, std::min(num1, num2)),
      upper(width, std::max(num1, num2));
  return llvm::ConstantRange(lower, upper);
}

llvm::Constant *
mutator_util::getRandomLLVMIntegerVector(llvm::FixedVectorType *ty) {
  assert(ty->getElementType()->isIntegerTy() && "expect an integer vector");
  llvm::IntegerType *intTy = (llvm::IntegerType *)ty->getElementType();
  unsigned length = ty->getNumElements();
  llvm::SmallVector<llvm::Constant *> result;
  for (size_t i = 0; i < length; ++i) {
    result.push_back(llvm::ConstantInt::get(intTy, getRandomLLVMInt(intTy)));
  }
  // we set threshold to 5, if p is less than threshold , we assign poison to
  // the vector
  unsigned threshold = 5, p = Random::getRandomUnsigned() % 100;
  if (p < threshold) {
    unsigned poisonNum = Random::getRandomUnsigned(2) + 1;
    poisonNum = std::min(poisonNum, length);
    for (size_t times = 0; times < poisonNum; ++times) {
      result[Random::getRandomUnsigned() % length] =
          llvm::PoisonValue::get(intTy);
    }
  }
  return llvm::ConstantVector::get(result);
}

llvm::Constant *mutator_util::updateIntegerVector(llvm::ConstantVector *ty) {
  unsigned length = ty->getType()->getNumElements();
  assert(ty->getType()->getElementType()->isIntegerTy() &&
         "only support for updating integer type");
  llvm::IntegerType *intTy =
      (llvm::IntegerType *)ty->getType()->getElementType();
  llvm::SmallVector<llvm::Constant *> result;
  for (size_t i = 0; i < length; ++i) {
    if (Random::getRandomBool()) {
      result.push_back(llvm::ConstantInt::get(intTy, getRandomLLVMInt(intTy)));
    } else {
      result.push_back(ty->getAggregateElement(i));
    }
  }
  return llvm::ConstantVector::get(result);
}

llvm::Value *mutator_util::insertGlobalVariable(llvm::Module *m,
                                                llvm::Type *ty) {
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

void mutator_util::propagateFunctionsInModule(llvm::Module *M, size_t num) {
  llvm::SmallVector<llvm::StringRef> funcNames;
  for (auto fit = M->begin(); fit != M->end(); ++fit) {
    if (!fit->isDeclaration()) {
      funcNames.push_back(fit->getName());
    }
  }
  for (size_t i = 0; i < funcNames.size(); ++i) {
    llvm::Function *func = M->getFunction(funcNames[i]);
    assert(func != nullptr && "cannot find function when propagate functions");
    llvm::ValueToValueMapTy vMap;
    for (size_t times = 0; times < num; ++times) {
      CloneFunction(func, vMap);
      vMap.clear();
    }
  }
}

std::string
mutator_util::insertFunctionArguments(llvm::Function *F,
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
  NewF->setName("aliveMutateGeneratedTmpFunction");
  std::string oldFuncName = F->getName().str(),
              newFuncName = NewF->getName().str();
  F->setName(newFuncName);
  NewF->setName(oldFuncName);
  return F->getName().str();
}

llvm::Value *mutator_util::updateIntegerSize(llvm::Value *integer,
                                             llvm::Type *newIntOrVecTy,
                                             llvm::Instruction *insertBefore) {
  assert(integer->getType()->isIntOrIntVectorTy() &&
         "should be a integer type to update the size");
  assert(newIntOrVecTy->isIntOrIntVectorTy() &&
         "should be an int or intVector type");
  bool isVec = newIntOrVecTy->isVectorTy();
  size_t oldSize = -1, newSize = -1;

  if (isVec) {
    oldSize = ((llvm::VectorType *)integer->getType())
                  ->getElementType()
                  ->getIntegerBitWidth();
    newSize = ((llvm::VectorType *)newIntOrVecTy)
                  ->getElementType()
                  ->getIntegerBitWidth();
  } else {
    oldSize = integer->getType()->getIntegerBitWidth();
    newSize = newIntOrVecTy->getIntegerBitWidth();
  }

  if (oldSize < newSize) {
    return llvm::CastInst::Create(llvm::Instruction::CastOps::ZExt, integer,
                                  newIntOrVecTy, "", insertBefore);
  } else if (oldSize > newSize) {
    return llvm::CastInst::Create(llvm::Instruction::CastOps::Trunc, integer,
                                  newIntOrVecTy, "", insertBefore);
  } else {
    return integer;
  }
}

void mutator_util::removeTBAAMetadata(llvm::Module *M) {
  for (auto fit = M->begin(); fit != M->end(); fit++) {
    if (!fit->isDeclaration() && !fit->getName().empty()) {
      for (auto iit = llvm::inst_begin(*fit); iit != llvm::inst_end(*fit);
           iit++) {
        iit->setMetadata("tbaa", nullptr);
      }
    }
  }
}

const std::vector<llvm::Instruction::BinaryOps> mutator_util::integerBinaryOps{
    llvm::Instruction::Add,  llvm::Instruction::Sub,  llvm::Instruction::Mul,
    llvm::Instruction::SDiv, llvm::Instruction::UDiv, llvm::Instruction::SRem,
    llvm::Instruction::URem, llvm::Instruction::Shl,  llvm::Instruction::LShr,
    llvm::Instruction::AShr, llvm::Instruction::And,  llvm::Instruction::Xor,
    llvm::Instruction::Or};

const std::vector<llvm::Instruction::BinaryOps> mutator_util::floatBinaryOps{
    llvm::Instruction::FAdd, llvm::Instruction::FSub, llvm::Instruction::FMul,
    llvm::Instruction::FDiv, llvm::Instruction::FRem};

const std::vector<llvm::Intrinsic::ID> mutator_util::integerBinaryIntrinsic{
    llvm::Intrinsic::IndependentIntrinsics::smax,
    llvm::Intrinsic::IndependentIntrinsics::smin,
    llvm::Intrinsic::IndependentIntrinsics::umax,
    llvm::Intrinsic::IndependentIntrinsics::umin,
    llvm::Intrinsic::IndependentIntrinsics::sadd_sat,
    llvm::Intrinsic::IndependentIntrinsics::uadd_sat,
    llvm::Intrinsic::IndependentIntrinsics::ssub_sat,
    llvm::Intrinsic::IndependentIntrinsics::usub_sat,
    llvm::Intrinsic::IndependentIntrinsics::sshl_sat,
    llvm::Intrinsic::IndependentIntrinsics::ushl_sat

};

const std::vector<llvm::Intrinsic::ID> mutator_util::floatBinaryIntrinsic{
    llvm::Intrinsic::IndependentIntrinsics::pow,
    llvm::Intrinsic::IndependentIntrinsics::minnum,
    llvm::Intrinsic::IndependentIntrinsics::maxnum,
    llvm::Intrinsic::IndependentIntrinsics::minimum,
    llvm::Intrinsic::IndependentIntrinsics::maximum,
    llvm::Intrinsic::IndependentIntrinsics::copysign};

const std::vector<llvm::Intrinsic::ID> mutator_util::integerUnaryIntrinsic{
    llvm::Intrinsic::IndependentIntrinsics::bitreverse,
    llvm::Intrinsic::IndependentIntrinsics::ctpop,
};

const std::vector<llvm::Intrinsic::ID> mutator_util::floatUnaryIntrinsic{
    llvm::Intrinsic::IndependentIntrinsics::sqrt,
    llvm::Intrinsic::IndependentIntrinsics::sin,
    llvm::Intrinsic::IndependentIntrinsics::cos,
    llvm::Intrinsic::IndependentIntrinsics::exp,
    llvm::Intrinsic::IndependentIntrinsics::exp2,
    llvm::Intrinsic::IndependentIntrinsics::log,
    llvm::Intrinsic::IndependentIntrinsics::log10,
    llvm::Intrinsic::IndependentIntrinsics::log2,
    llvm::Intrinsic::IndependentIntrinsics::fabs,
    llvm::Intrinsic::IndependentIntrinsics::floor,
    llvm::Intrinsic::IndependentIntrinsics::ceil,
    llvm::Intrinsic::IndependentIntrinsics::trunc,
    llvm::Intrinsic::IndependentIntrinsics::rint,
    llvm::Intrinsic::IndependentIntrinsics::nearbyint,
    llvm::Intrinsic::IndependentIntrinsics::round,
    llvm::Intrinsic::IndependentIntrinsics::roundeven,
    llvm::Intrinsic::IndependentIntrinsics::canonicalize};

llvm::Instruction *
mutator_util::getRandomIntegerInstruction(llvm::Value *val1, llvm::Value *val2,
                                          llvm::Instruction *insertBefore) {
  assert(val1->getType()->isIntOrIntVectorTy() &&
         "should be an integer to get an int instruction!");
  assert(val2->getType()->isIntOrIntVectorTy() &&
         "should be an integer to get an int instruction!");
  return Random::getRandomBool()
             ? getRandomIntegerBinaryInstruction(val1, val2, insertBefore)
             : getRandomIntegerIntrinsic(val1, val2, insertBefore);
}

llvm::Instruction *
mutator_util::getRandomFloatInstruction(llvm::Value *val1, llvm::Value *val2,
                                        llvm::Instruction *insertBefore) {
  assert(val1->getType()->isFPOrFPVectorTy() &&
         "should be a floating point to get a float instruction!");
  assert(val2->getType()->isFPOrFPVectorTy() &&
         "should be a floating point to get a float instruction!");
  return Random::getRandomBool()
             ? getRandomFloatBinaryInstruction(val1, val2, insertBefore)
             : getRandomFloatIntrinsic(val1, val2, insertBefore);
}

llvm::Instruction *mutator_util::getRandomIntegerBinaryInstruction(
    llvm::Value *val1, llvm::Value *val2, llvm::Instruction *insertBefore) {
  assert(val1->getType()->isIntOrIntVectorTy() &&
         "should be an integer to get an int instruction!");
  assert(val2->getType()->isIntOrIntVectorTy() &&
         "should be an integer to get an int instruction!");
  llvm::Instruction::BinaryOps Op =
      integerBinaryOps[Random::getRandomUnsigned() % integerBinaryOps.size()];
  return llvm::BinaryOperator::Create(Op, val1, val2, "", insertBefore);
}

llvm::Instruction *mutator_util::getRandomFloatBinaryInstruction(
    llvm::Value *val1, llvm::Value *val2, llvm::Instruction *insertBefore) {
  assert(val1->getType()->isFPOrFPVectorTy() &&
         "should be a floating point to get a float instruction!");
  assert(val2->getType()->isFPOrFPVectorTy() &&
         "should be a floating point to get a float instruction!");
  llvm::Instruction::BinaryOps Op =
      floatBinaryOps[Random::getRandomUnsigned() % floatBinaryOps.size()];
  return llvm::BinaryOperator::Create(Op, val1, val2, "", insertBefore);
}

llvm::Instruction *
mutator_util::getRandomIntegerIntrinsic(llvm::Value *val1, llvm::Value *val2,
                                        llvm::Instruction *insertBefore) {
  std::vector<llvm::Type *> tys{val1->getType()};
  llvm::Module *M = insertBefore->getModule();
  size_t pos = Random::getRandomUnsigned() %
               (integerUnaryIntrinsic.size() + integerBinaryIntrinsic.size());
  bool isUnary = pos < integerUnaryIntrinsic.size();
  llvm::Function *func = nullptr;
  std::vector<llvm::Value *> args{val1};
  if (isUnary) {
    func = llvm::Intrinsic::getDeclaration(M, integerUnaryIntrinsic[pos], tys);
  } else {
    pos -= integerUnaryIntrinsic.size();
    func = llvm::Intrinsic::getDeclaration(M, integerBinaryIntrinsic[pos], tys);
    args.push_back(val2);
  }
  assert(func != nullptr && "intrinsic function shouldn't be nullptr!");
  llvm::CallInst *inst = llvm::CallInst::Create(func->getFunctionType(), func,
                                                args, "", insertBefore);
  return inst;
}

llvm::Instruction *
mutator_util::getRandomFloatIntrinsic(llvm::Value *val1, llvm::Value *val2,
                                      llvm::Instruction *insertBefore) {
  std::vector<llvm::Type *> tys{val1->getType()};
  llvm::Module *M = insertBefore->getModule();
  size_t pos = Random::getRandomUnsigned() %
               (floatUnaryIntrinsic.size() + floatBinaryIntrinsic.size());
  bool isUnary = pos < floatUnaryIntrinsic.size();
  llvm::Function *func = nullptr;
  std::vector<llvm::Value *> args{val1};
  if (isUnary) {
    func = llvm::Intrinsic::getDeclaration(M, floatUnaryIntrinsic[pos], tys);
  } else {
    pos -= floatUnaryIntrinsic.size();
    func = llvm::Intrinsic::getDeclaration(M, floatBinaryIntrinsic[pos], tys);
    args.push_back(val2);
  }
  assert(func != nullptr && "intrinsic function shouldn't be nullptr!");
  llvm::CallInst *inst = llvm::CallInst::Create(func->getFunctionType(), func,
                                                args, "", insertBefore);
  return inst;
}
