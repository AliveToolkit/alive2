#include "mutator.h"
#include "mutator_helper.h"
#include "llvm/Support/CommandLine.h"

void StubMutator::moveToNextInst() {
  ++iit;
  if (iit == bit->end()) {
    moveToNextBlock();
  }
}

void StubMutator::moveToNextBlock() {
  ++bit;
  if (bit == fit->end()) {
    moveToNextFunction();
  } else {
    iit = bit->begin();
  }
}

void StubMutator::moveToNextFunction() {
  ++fit;
  while (fit == pm->end() || fit->isDeclaration()) {
    if (fit == pm->end()) {
      fit = pm->begin();
    } else {
      ++fit;
    }
  }
  bit = fit->begin();
  iit = bit->begin();
  currFunction = fit->getName();
}

bool StubMutator::init() {
  fit = pm->begin();
  while (fit != pm->end()) {
    bit = fit->begin();
    while (bit != fit->end()) {
      iit = bit->begin();
      if (iit == bit->end()) {
        ++bit;
        continue;
      }
      currFunction = fit->getName();
      return true;
    }
    ++fit;
  }
  return false;
}

void StubMutator::mutateModule(const std::string &outputFileName) {
  if (debug) {
    llvm::errs() << "current inst:\n";
    iit->print(llvm::errs());
    llvm::errs() << "\n";
  }
  llvm::errs() << "stub mutation visited.\n";
  if (debug) {
    llvm::errs() << "current basic block\n";
    bit->print(llvm::errs());
    std::error_code ec;
    llvm::raw_fd_ostream fout(outputFileName, ec);
    fout << *pm;
    fout.close();
    llvm::errs() << "file wrote to " << outputFileName << "\n";
  }
  moveToNextInst();
}

void StubMutator::saveModule(const std::string &outputFileName) {
  std::error_code ec;
  llvm::raw_fd_ostream fout(outputFileName, ec);
  fout << *pm;
  fout.close();
  llvm::errs() << "file wrote to " << outputFileName << "\n";
}

std::string StubMutator::getCurrentFunction() const {
  return currFunction;
}

bool Mutator::openInputFile(const string &inputFile) {
  auto MB =
      ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(inputFile)));
  llvm::SMDiagnostic Diag;
  pm = getLazyIRModule(std::move(MB), Diag, context,
                       /*ShouldLazyLoadMetadata=*/true);
  if (!pm) {
    Diag.print("", llvm::errs(), false);
    return false;
  }
  ExitOnErr(pm->materializeAll());
  return true;
}

void FunctionMutator::print() {
  llvm::errs() << "Current function " << getCurrentFunction()->getName()
               << "\n";
  llvm::errs() << "Current basic block:\n";
  bitInTmp->print(llvm::errs());
  llvm::errs() << "\nCurrent instruction:\n";
  iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
}

extern llvm::cl::list<size_t> disableSEXT;
extern llvm::cl::list<size_t> disableZEXT;
extern llvm::cl::list<size_t> disableEXT;

void FunctionMutator::init(std::shared_ptr<FunctionMutator> self) {
  for (llvm::inst_iterator it = inst_begin(*currentFunction);
       it != inst_end(*currentFunction); ++it) {
    for (size_t i = 0; i < it->getNumOperands(); ++i) {
      if (llvm::Value *val = it->getOperand(i);
          val != nullptr && llvm::isa<llvm::ConstantInt>(*val)) {
        Random::addUsedInt(((llvm::ConstantInt *)val)->getLimitedValue());
      }
    }
  }

  if (FunctionAttributeHelper::canMutate(currentFunction)) {
    std::unordered_set<size_t> disableSEXTset, disableZEXTset, disableEXTset;
    for (size_t bitWidth : disableSEXT) {
      disableSEXTset.insert(bitWidth);
    }
    for (size_t bitWidth : disableZEXT) {
      disableZEXTset.insert(bitWidth);
    }
    for (size_t bitWidth : disableEXT) {
      disableEXTset.insert(bitWidth);
    }
    helpers.push_back(std::make_unique<FunctionAttributeHelper>(
        self, disableSEXTset, disableZEXTset, disableEXTset));
    whenMoveToNextFuncFuncs.push_back(helpers.size() - 1);
  }

  if (ShuffleHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<ShuffleHelper>(self));
    whenMoveToNextFuncFuncs.push_back(helpers.size() - 1);
    whenMoveToNextBasicBlockFuncs.push_back(helpers.size() - 1);
  }

  if (RandomMoveHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<RandomMoveHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  if (ResizeIntegerHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<ResizeIntegerHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  if (BinaryInstructionHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<BinaryInstructionHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  if (MutateInstructionHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<MutateInstructionHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  /*if (RandomCodeInserterHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<RandomCodeInserterHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }*/

  if (GEPHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<GEPHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  if (FunctionCallInlineHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<FunctionCallInlineHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  if (VoidFunctionCallRemoveHelper::canMutate(currentFunction)) {
    helpers.push_back(std::make_unique<VoidFunctionCallRemoveHelper>(self));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);
  }

  for (size_t i = 0; i < helpers.size(); ++i) {
    helpers[i]->init();
  }
}

void FunctionMutator::resetIterator() {
  bit = currentFunction->begin();
  iit = bit->begin();
  initAtFunctionEntry();
}

void FunctionMutator::resetRandomIterator(){
  bit=currentFunction->begin();
  for(size_t i=Random::getRandomUnsigned()%currentFunction->size();i;--i,++bit);
  iit=bit->begin();
  for(size_t i=Random::getRandomUnsigned()%bit->size();i;--i,++iit);
  initAtNewBasicBlock();
  initAtFunctionEntry();
  moveToNextMutant();
  bitInTmp=((llvm::BasicBlock*)&*vMap[&*bit])->getIterator();
  iitInTmp=((llvm::Instruction*)&*vMap[&*iit])->getIterator();
}

bool FunctionMutator::canMutate(const llvm::Instruction &inst,
                                const llvm::StringSet<> &filterSet) {
  // contain immarg attributes
  if (llvm::isa<llvm::CallBase>(&inst)) {
    // in case of cannot find function name
    if (llvm::Function *func = ((llvm::CallBase *)&inst)->getCalledFunction();
        func != nullptr &&
        (filterSet.find(func->getName().str()) != filterSet.end() ||
         func->getName().startswith("llvm"))) {
      return false;
    }
  }
  // don't do replacement on PHI node
  // don't update an alloca inst
  // don't do operations on Switch inst for now.
  if (llvm::isa<llvm::PHINode>(inst) || llvm::isa<llvm::AllocaInst>(inst) ||
      llvm::isa<llvm::SwitchInst>(inst)) {

    return false;
  }

  return true;
}

bool FunctionMutator::canMutate(const llvm::BasicBlock &block,
                                const llvm::StringSet<> &filterSet) {
  return !block.instructionsWithoutDebug().empty() &&
         std::any_of(block.begin(), block.end(),
                     [&filterSet](const llvm::Instruction &inst) {
                       return canMutate(inst, filterSet);
                     });
}

bool FunctionMutator::canMutate(const llvm::Function *function,
                                const llvm::StringSet<> &filterSet) {
  return !function->empty() &&
         std::any_of(function->begin(), function->end(),
                     [&filterSet](const llvm::BasicBlock &bb) {
                       return canMutate(bb, filterSet);
                     });
}

bool FunctionMutator::checkValid() {
  std::string str;
  llvm::raw_string_ostream sout(str);
  if (llvm::verifyFunction(*functionInTmp, (llvm::raw_ostream *)&sout)) {
    // fix mustfail
    if (sout.str().find("musttail") != std::string::npos) {
      for (auto bb_it = functionInTmp->begin(); bb_it != functionInTmp->end();
           ++bb_it) {
        if (llvm::isa<llvm::ReturnInst>(bb_it->getTerminator())) {
          llvm::ReturnInst *ret = (llvm::ReturnInst *)bb_it->getTerminator();
          llvm::Value *retVal = ret->getReturnValue();
          if (retVal == nullptr) {
            continue;
          }
          llvm::Value *musttailCall = nullptr;
          if (llvm::isa<llvm::Instruction>(retVal)) {
            if (llvm::isa<llvm::BitCastInst>(retVal)) {
              llvm::BitCastInst *bitcast = (llvm::BitCastInst *)retVal;
              retVal = bitcast->getOperand(0);
            }
            if (llvm::isa<llvm::CallInst>(retVal)) {
              musttailCall = retVal;
            }
          }
          for (auto inst_it = bb_it->begin(); inst_it != bb_it->end();
               ++inst_it) {
            if (llvm::isa<llvm::CallInst>(*inst_it)) {
              llvm::CallInst *callInst = (llvm::CallInst *)&*inst_it;
              if (callInst != musttailCall &&
                  callInst->getTailCallKind() ==
                      llvm::CallInst::TailCallKind::TCK_MustTail) {
                callInst->setTailCallKind(
                    llvm::CallInst::TailCallKind::TCK_None);
              }
            }
          }
        }
      }
      return true;
    }
  }
  if(!sout.str().empty()){
    llvm::errs()<<"Verify message: "<<sout.str()<<"\n";
  }
  return sout.str().empty();
}

void FunctionMutator::mutate() {
  if (debug) {
    print();
  }

  bool mutated = false, canMutate = false;
  do {
    for (size_t i = 0; i < helpers.size(); ++i) {
      if (helpers[i]->shouldMutate()) {
        canMutate = true;
        if (Random::getRandomBool()) {
          helpers[i]->mutate();
          mutated = true;
          if (debug) {
            helpers[i]->debug();
          }
        }
      }
    }
  } while (!mutated && canMutate);

  if(!checkValid()){
    llvm::errs()<<"Invalid LLVM IR generated. Current seed: "<<Random::getSeed()<<"\n";
    llvm::errs()<<"Current Module Identifier: "<<currentFunction->getParent()->getModuleIdentifier()<<"\n";
    llvm::errs()<<"Current Function:\n";
    functionInTmp->print(llvm::errs());
    llvm::errs()<<"\n";
    print();
    llvm::errs()<<"End\n";
  }
  if (debug) {
    print();
  }
  moveToNextMutant();
}

void FunctionMutator::moveToNextInstruction() {
  ++iit;
  if (iit == bit->end()) {
    moveToNextBasicBlock();
  }
}

void FunctionMutator::initAtNewBasicBlock() {
  for (size_t i : whenMoveToNextBasicBlockFuncs) {
    helpers[i]->whenMoveToNextBasicBlock();
  }
}

void FunctionMutator::initAtNewInstruction() {
  for (size_t i : whenMoveToNextInstFuncs) {
    helpers[i]->whenMoveToNextInst();
  }
}

void FunctionMutator::initAtFunctionEntry() {
  for (size_t i : whenMoveToNextFuncFuncs) {
    helpers[i]->whenMoveToNextFunction();
  }
}

void FunctionMutator::moveToNextBasicBlock() {
  ++bit;
  initAtNewBasicBlock();
  if (bit == currentFunction->end()) {
    resetIterator();
  } else {
    iit = bit->begin();
  }
}

void FunctionMutator::moveToNextMutant() {
  moveToNextInstruction();
  while (!canMutate(*iit, filterSet))
    moveToNextInstruction();
  initAtNewInstruction();
}

void FunctionMutator::resetTmpCopy(std::shared_ptr<llvm::Module> copy) {
  extraValues.clear();
  invalidValues.clear();
  tmpFuncs.clear();
  tmpCopy = copy;
  assert(vMap.find(currentFunction) != vMap.end() &&
         vMap.find(&*bit) != vMap.end() && vMap.find(&*iit) != vMap.end() &&
         "vMap is invalid!");
  functionInTmp =
      &*llvm::Module::iterator((llvm::Function *)&*vMap[currentFunction]);
  bitInTmp = llvm::Function::iterator((llvm::BasicBlock *)&*vMap[&*bit]);
  iitInTmp = llvm::BasicBlock::iterator((llvm::Instruction *)&*vMap[&*iit]);
}

void FunctionMutator::setOperandRandomValue(llvm::Instruction *inst,
                                            size_t pos) {
  if (llvm::Type *ty = inst->getOperand(pos)->getType(); ty != nullptr) {
    if (llvm::Value *val = getRandomValue(ty);
        Random::getRandomBool() || val == nullptr ||
        llvm::isa<llvm::UndefValue>(val)) {
      lazyUpdateInsts.push_back(inst);
      lazyUpdateArgPos.push_back(pos);
      lazyUpdateArgTys.push_back(inst->getOperand(pos)->getType());
      inst->setOperand(pos, llvm::UndefValue::get(ty));
    } else {
      inst->setOperand(pos, val);
    }
  }
}

void FunctionMutator::addFunctionArguments(
    const llvm::SmallVector<llvm::Type *> &tys, llvm::ValueToValueMapTy &VMap) {
  if (!lazyUpdateInsts.empty()) {
    size_t oldArgSize = functionInTmp->arg_size();
    std::string oldFuncName =
        mutator_util::insertFunctionArguments(functionInTmp, tys, VMap);
    tmpFuncs.push_back(std::move(oldFuncName));
    iitInTmp = ((llvm::Instruction *)&*VMap[&*iitInTmp])->getIterator();
    bitInTmp = ((llvm::BasicBlock *)&*VMap[&*bitInTmp])->getIterator();
    functionInTmp = bitInTmp->getParent();
    for (size_t i = 0; i < lazyUpdateInsts.size(); ++i) {
      lazyUpdateInsts[i] = (llvm::Instruction *)&*VMap[lazyUpdateInsts[i]];
    }

    for (auto it = vMap.begin(); it != vMap.end(); ++it) {
      if (VMap.find(it->second) != VMap.end()) {
        it->second = VMap[it->second];
      }
    }
    for (size_t i = 0; i < tys.size(); ++i) {
      extraValues.push_back(functionInTmp->getArg(i + oldArgSize));
    }
  }
}

void FunctionMutator::fixAllValues(llvm::SmallVector<llvm::Value *> &vals) {
  if (!lazyUpdateInsts.empty()) {
    llvm::ValueToValueMapTy VMap;
    llvm::DenseSet<llvm::Instruction*> newInvalidValues;
    addFunctionArguments(lazyUpdateArgTys, VMap);
    for (size_t i = 0; i < extraValues.size(); ++i) {
      if (VMap.find(extraValues[i]) != VMap.end()) {
        extraValues[i] = (llvm::Value *)&*VMap[extraValues[i]];
      }
    }
    for(auto it=invalidValues.begin();it!=invalidValues.end();++it){
      if(VMap.find(*it)!=VMap.end()){
        newInvalidValues.insert((llvm::Instruction*)&*VMap[*it]);
      }
    }
    invalidValues=std::move(newInvalidValues);
    for (size_t i = 0; i < vals.size(); ++i) {
      vals[i] = (llvm::Value *)&*VMap[vals[i]];
    }

    for (size_t i = 0; i < lazyUpdateInsts.size(); ++i) {
      lazyUpdateInsts[i]->setOperand(lazyUpdateArgPos[i],
                                     getRandomValue(lazyUpdateArgTys[i]));
    }
    lazyUpdateArgTys.clear();
    lazyUpdateArgPos.clear();
    lazyUpdateInsts.clear();
  }
}

llvm::Value *FunctionMutator::getRandomConstant(llvm::Type *ty) {
  if (ty->isIntegerTy()) {
    return llvm::ConstantInt::get(ty, Random::getRandomUnsigned());
  }
  return llvm::UndefValue::get(ty);
}

llvm::Value *FunctionMutator::getRandomDominatedInstruction(llvm::Type *ty) {
  if(ty!=nullptr){
    auto bbIt=currentFunction->begin();
    auto instIt=iit;
    bool isIntTy = ty->isIntegerTy();
    for(size_t pos=Random::getRandomUnsigned()%currentFunction->size();pos;--pos,++bbIt);
    for(size_t i=0;i<currentFunction->size();++i,++bbIt){
      if(bbIt==currentFunction->end()){
        bbIt=currentFunction->begin();
      }
      if(DT.dominates(&*bbIt, &*bit) || bbIt==bit){
        instIt=bbIt->begin();
        for(size_t pos=Random::getRandomUnsigned()%bbIt->size();pos;--pos,++instIt);
        for(size_t inner_i=0;inner_i<bbIt->size();++inner_i,++instIt){
          if(instIt==bbIt->end()){
            instIt=bbIt->begin();
          }
          if(invalidValues.contains((llvm::Instruction*)&*vMap[&*instIt])){
            continue;
          }
          if(DT.dominates(&*instIt, &*iit)){
            if(instIt->getType()==ty){
              return &*vMap[&*instIt];
            }else if(isIntTy && instIt->getType()->isIntegerTy()){
              llvm::Value *valInTmp = &*vMap[&*instIt];
              llvm::Instruction *insertBefore = &*iitInTmp;
              return mutator_util::updateIntegerSize(
                  valInTmp, (llvm::IntegerType *)ty, &*insertBefore);
            }
          }
        }
      }
    }
  }
  return nullptr;
}

llvm::Value *FunctionMutator::getRandomArgument(llvm::Type *ty) {
  if(currentFunction->arg_size()!=0){
    bool isIntTy = ty->isIntegerTy();
    for(size_t i=0,pos=Random::getRandomUnsigned()%currentFunction->arg_size();
         i<currentFunction->arg_size();++i,++pos){
      if(pos==currentFunction->arg_size()){
        pos=0;
      }
      llvm::Argument* curArg=currentFunction->getArg(pos);
      if(currentFunction->getArg(pos)->getType()==ty){
        return &*vMap[curArg];
      }else if (isIntTy && curArg->getType()->isIntegerTy()) {
        llvm::Instruction *insertBefore = &*functionInTmp->begin()->begin();
        llvm::Value *valInTmp = &*vMap[curArg];
        return mutator_util::updateIntegerSize(
            valInTmp, (llvm::IntegerType *)ty, &*insertBefore);
      }
    }
  }
  return nullptr;
}

llvm::Value *FunctionMutator::getRandomValueFromExtraValue(llvm::Type *ty) {
  if (ty != nullptr && !extraValues.empty()) {
    for(size_t i=0,pos=Random::getRandomUnsigned()%extraValues.size();i<extraValues.size();++i,++pos){
      if(pos==extraValues.size()){
        pos=0;
      }
      if(llvm::Instruction* inst=llvm::dyn_cast<llvm::Instruction>(extraValues[pos]); inst){
        if(!invalidValues.contains(inst) && inst->getType()==ty){
          return extraValues[pos];
        }
      }else if(extraValues[pos]->getType()==ty){
        return extraValues[pos];
      }
    }
  }
  return nullptr;
}

llvm::Value *FunctionMutator::getRandomPointerValue(llvm::Type *ty) {
  if (ty->isPointerTy()) {
    if (Random::getRandomUnsigned() % (1 + valueFuncs.size()) == 0) {
      return llvm::ConstantPointerNull::get((llvm::PointerType *)(ty));
    }
    return getRandomValue(ty);
  }
  return nullptr;
}

llvm::Value *FunctionMutator::getRandomValue(llvm::Type *ty) {
  if (ty != nullptr && !valueFuncs.empty()) {
    bool isUndef = false;
    for (size_t i = 0, pos = Random::getRandomUnsigned() % valueFuncs.size();
         i < valueFuncs.size(); ++i, ++pos) {
      if (pos == valueFuncs.size())
        pos = 0;
      if (llvm::Value *result = (this->*valueFuncs[pos])(ty);
          result != nullptr) {
        if (llvm::isa<llvm::UndefValue>(result)) {
          isUndef = true;
          continue;
        }
        return result;
      }
    }
    if (isUndef) {
      return llvm::UndefValue::get(ty);
    }
  }
  return nullptr;
}

llvm::Value *FunctionMutator::getRandomFromGlobal(llvm::Type *ty) {
  if (ty != nullptr && !globals.empty()) {
    return mutator_util::findRandomInArray<llvm::Value *, llvm::Type *>(
        extraValues, ty,
        [](llvm::Value *v, llvm::Type *ty) { return v->getType() == ty; },
        nullptr);
  }
  return nullptr;
}

void FunctionMutator::removeTmpFunction() {
  for (const auto &funcName : tmpFuncs) {
    if (llvm::Function *func = tmpCopy->getFunction(funcName);
        func != nullptr && func->use_empty()) {
      func->eraseFromParent();
    }
  }
}

static bool hasUndefOperand(llvm::Instruction *inst) {
  return std::any_of(inst->op_begin(), inst->op_end(), [](llvm::Use &use) {
    return llvm::isa<llvm::UndefValue>(use.get());
  });
}

void FunctionMutator::removeAllUndef() {
  resetIterator();
  llvm::SmallVector<llvm::Value *> vec;
  for (auto it = inst_begin(functionInTmp); it != inst_end(functionInTmp);
       ++it) {
    if (hasUndefOperand(&*it)) {
      vec.push_back(&*it);
    }
  }
  std::reverse(vec.begin(), vec.end());
  while (!vec.empty()) {
    llvm::Instruction *inst = (llvm::Instruction *)vec.back();
    vec.pop_back();
    llvm::SmallVector<size_t> pos;
    for (size_t i = 0; i < inst->getNumOperands(); ++i) {
      if (llvm::isa<llvm::UndefValue>(inst->getOperand(i))) {
        pos.push_back(i);
      }
    }
    for (size_t i : pos) {
      setOperandRandomValue(inst, i);
    }
    fixAllValues(vec);
  }
}

bool ModuleMutator::init() {
  for (auto fit = pm->begin(); fit != pm->end(); ++fit) {
    for (auto ait = fit->arg_begin(); ait != fit->arg_end(); ++ait) {
      //ignore functions with immarg attributes
      if (ait->hasAttribute(llvm::Attribute::AttrKind::ImmArg)) {
        filterSet.insert(fit->getName());
      }
      //ignore functions with swifterror attributes
      if(ait->hasAttribute(llvm::Attribute::AttrKind::SwiftError)){
        filterSet.insert(fit->getName());
      }
    }
  }
  for (auto fit = pm->begin(); fit != pm->end(); ++fit) {
    if (!fit->isDeclaration() && !fit->getName().empty() &&
        !invalidFunctions.contains(fit->getName())) {
      if (FunctionMutator::canMutate(&*fit, filterSet)) {
        functionMutants.push_back(std::make_shared<FunctionMutator>(
            &*fit, vMap, filterSet, globals, debug));
      }
    }
  }
  for (size_t i = 0; i < functionMutants.size(); ++i) {
    functionMutants[i]->init(functionMutants[i]);
  }
  if (!functionMutants.empty()) {
    for (auto git = pm->global_begin(); git != pm->global_end(); ++git) {
      globals.push_back(&*git);
      for (auto oit = git->op_begin(); oit != git->op_end(); ++oit) {
        if (oit->get() != nullptr && llvm::isa<llvm::ConstantInt>(*oit)) {
          Random::addUsedInt(
              llvm::cast<llvm::ConstantInt>(*oit)->getLimitedValue());
        }
      }
      if (llvm::isa<llvm::ConstantInt>(*git)) {
        Random::addUsedInt(
            llvm::cast<llvm::ConstantInt>(*git).getLimitedValue());
      }
    }
    return true;
  }
  return false;
}

void ModuleMutator::resetTmpModule() {
  vMap.clear();
  tmpCopy = llvm::CloneModule(*pm, vMap);
  for (size_t i = 0; i < functionMutants.size(); ++i) {
    functionMutants[i]->resetTmpCopy(tmpCopy);
  }
}

void ModuleMutator::mutateModule(const std::string &outputFileName) {
  resetTmpModule();
  assert(curFunction < functionMutants.size() &&
         "curFunction should be a valid function");

  if (onEveryFunction) {
    for (size_t i = 0; i < functionMutants.size(); ++i) {
      functionMutants[i]->mutate();
      functionMutants[i]->removeTmpFunction();
    }
  } else {
    if(randomMutate){
      curFunction = Random::getRandomUnsigned() % functionMutants.size();
      functionMutants[curFunction]->resetRandomIterator();
    }
    functionMutants[curFunction]->mutate();
    curFunctionName =
        functionMutants[curFunction]->getCurrentFunction()->getName();
    functionMutants[curFunction]->removeTmpFunction();
  }

  ++curFunction;
  if (curFunction == functionMutants.size()) {
    curFunction = 0;
  }
}

void ModuleMutator::saveModule(const std::string &outputFileName) {
  std::error_code ec;
  llvm::raw_fd_ostream fout(outputFileName, ec);
  fout<<";"<<curFunctionName<<"\n";
  fout<<";current seed:" << Random::getSeed()<<"\n";
  fout << *tmpCopy;
  fout.close();
  if (debug) {
    llvm::errs() << "file wrote to " << outputFileName << "\n";
  }
}
