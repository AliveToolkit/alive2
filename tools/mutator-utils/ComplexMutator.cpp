#include "ComplexMutator.h"

void FunctionMutant::debug() {
  llvm::errs() << "Current function " << getCurrentFunction()->getName()
               << "\n";
  llvm::errs() << "Current basic block:\n";
  bitInTmp->print(llvm::errs());
  llvm::errs() << "\nCurrent instruction:\n";
  iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
}

void FunctionMutant::init(std::shared_ptr<FunctionMutant> self) {
  for (llvm::inst_iterator it = inst_begin(*currentFunction);
       it != inst_end(*currentFunction); ++it) {
    for (size_t i = 0; i < it->getNumOperands(); ++i) {
      if (llvm::Value *val = it->getOperand(i);
          val != nullptr && llvm::isa<ConstantInt>(*val)) {
        Random::addUsedInt(((llvm::ConstantInt *)val)->getLimitedValue());
      }
    }
  }

  helpers.push_back(std::make_unique<ShuffleHelper>(self));
  whenMoveToNextFuncFuncs.push_back(helpers.size() - 1);
  whenMoveToNextBasicBlockFuncs.push_back(helpers.size() - 1);

  helpers.push_back(std::make_unique<RandomMoveHelper>(self));
  whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

  helpers.push_back(std::make_unique<MutateInstructionHelper>(self));
  whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

  helpers.push_back(std::make_unique<RandomCodeInserterHelper>(self));
  whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

  helpers.push_back(std::make_unique<FunctionCallInlineHelper>(self));
  whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

  for (size_t i = 0; i < helpers.size(); ++i) {
    helpers[i]->init();
  }
}

void FunctionMutant::resetIterator() {
  bit = currentFunction->begin();
  iit = bit->begin();
  initAtFunctionEntry();
}

bool FunctionMutant::canMutate(const llvm::Instruction &inst,
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
  if (llvm::isa<llvm::PHINode>(inst) ||
      llvm::isa<llvm::GetElementPtrInst>(inst) ||
      llvm::isa<llvm::AllocaInst>(inst) || llvm::isa<llvm::SwitchInst>(inst)) {

    return false;
  }

  return true;
}

bool FunctionMutant::canMutate(const llvm::BasicBlock &block,
                               const llvm::StringSet<> &filterSet) {
  return !block.getInstList().empty() &&
         std::all_of(block.begin(), block.end(),
                     [&filterSet](const llvm::Instruction &inst) {
                       return canMutate(inst, filterSet);
                     });
}

bool FunctionMutant::canMutate(const llvm::Function *function,
                               const llvm::StringSet<> &filterSet) {
  return !function->getBasicBlockList().empty() &&
         std::all_of(function->begin(), function->end(),
                     [&filterSet](const llvm::BasicBlock &bb) {
                       return canMutate(bb, filterSet);
                     });
}

void FunctionMutant::mutate() {
  for (size_t i = 0; i < helpers.size(); ++i) {
    if (helpers[i]->shouldMutate()) {
      helpers[i]->mutate();
    }
  }
  moveToNextMutant();
}

void FunctionMutant::moveToNextInstruction() {
  assert(domVals.inBackup());
  domVals.push_back(&*iit);
  ++iit;
  if (iit == bit->end()) {
    moveToNextBasicBlock();
  }
}

void FunctionMutant::initAtNewBasicBlock() {
  for (size_t i : whenMoveToNextBasicBlockFuncs) {
    helpers[i]->whenMoveToNextBasicBlock();
  }
}

void FunctionMutant::initAtNewInstruction() {
  for (size_t i : whenMoveToNextInstFuncs) {
    helpers[i]->whenMoveToNextInst();
  }
}

void FunctionMutant::initAtFunctionEntry() {
  for (size_t i : whenMoveToNextFuncFuncs) {
    helpers[i]->whenMoveToNextFunction();
  }
}

void FunctionMutant::moveToNextBasicBlock() {
  ++bit;
  if (bit == currentFunction->end()) {
    resetIterator();
  } else {
    iit = bit->begin();
  }
  calcDomVals();
  initAtNewBasicBlock();
}

void FunctionMutant::moveToNextMutant() {
  moveToNextInstruction();
  while (!canMutate(*iit, filterSet))
    moveToNextInstruction();
  domVals.restoreBackup();
  initAtNewInstruction();
}

void FunctionMutant::calcDomVals() {
  domVals.deleteBackup();
  domVals.resize(currentFunction->arg_size());
  // add BasicBlocks before bitTmp
  for (auto bitTmp = currentFunction->begin(); bitTmp != bit; ++bitTmp) {
    if (DT.dominates(&*bitTmp, &*bit)) {
      for (auto iitTmp = bitTmp->begin(); iitTmp != bitTmp->end(); ++iitTmp) {
        domVals.push_back(&*iitTmp);
      }
    }
  }
  domVals.startBackup();
  // add Instructions before iitTmp
  for (auto iitTmp = bit->begin(); iitTmp != iit; ++iitTmp) {
    if (DT.dominates(&*iitTmp, &*iit)) {
      domVals.push_back(&*iitTmp);
    }
  }
}

void FunctionMutant::resetTmpCopy(std::shared_ptr<llvm::Module> copy) {
  extraValues.clear();
  tmpCopy = copy;
  assert(vMap.find(currentFunction) != vMap.end() &&
         vMap.find(&*bit) != vMap.end() && vMap.find(&*iit) != vMap.end() &&
         "vMap is invalid!");
  functionInTmp =
      &*llvm::Module::iterator((llvm::Function *)&*vMap[currentFunction]);
  bitInTmp = llvm::Function::iterator((llvm::BasicBlock *)&*vMap[&*bit]);
  iitInTmp = llvm::BasicBlock::iterator((llvm::Instruction *)&*vMap[&*iit]);
}

void FunctionMutant::setOperandRandomValue(llvm::Instruction *inst,
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

void FunctionMutant::addFunctionArguments(
    const llvm::SmallVector<llvm::Type *> &tys, llvm::ValueToValueMapTy &VMap) {
  if (!lazyUpdateInsts.empty()) {
    size_t oldArgSize = functionInTmp->arg_size();
    LLVMUtil::insertFunctionArguments(functionInTmp, tys, VMap);
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

void FunctionMutant::fixAllValues(llvm::SmallVector<llvm::Value *> &vals) {
  if (!lazyUpdateInsts.empty()) {
    llvm::ValueToValueMapTy VMap;
    addFunctionArguments(lazyUpdateArgTys, VMap);
    for (size_t i = 0; i < extraValues.size(); ++i) {
      if (VMap.find(extraValues[i]) != VMap.end()) {
        extraValues[i] = (llvm::Value *)&*VMap[extraValues[i]];
      }
    }
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

llvm::Value *FunctionMutant::getRandomConstant(llvm::Type *ty) {
  if (ty->isIntegerTy()) {
    return llvm::ConstantInt::get(ty, Random::getRandomUnsigned());
  }
  return llvm::UndefValue::get(ty);
}

llvm::Value *FunctionMutant::getRandomDominatedValue(llvm::Type *ty) {
  if (ty != nullptr && !domVals.empty()) {
    for (size_t i = 0, pos = Random::getRandomUnsigned() % domVals.size();
         i < domVals.size(); ++i, ++pos) {
      if (pos == domVals.size())
        pos = 0;
      if (domVals[pos]->getType() == ty) {
        return &*vMap[domVals[pos]];
      }
    }
  }
  return nullptr;
}

llvm::Value *FunctionMutant::getRandomValueFromExtraValue(llvm::Type *ty) {
  if (ty != nullptr && !extraValues.empty()) {
    return LLVMUtil::findRandomInArray<llvm::Value *, llvm::Type *>(
        extraValues, ty,
        [](llvm::Value *v, llvm::Type *ty) { return v->getType() == ty; },
        nullptr);
  }
  return nullptr;
}

llvm::Value *FunctionMutant::getRandomPointerValue(llvm::Type *ty) {
  if (ty->isPointerTy()) {
    if (Random::getRandomUnsigned() % (1 + valueFuncs.size()) == 0) {
      return llvm::ConstantPointerNull::get((llvm::PointerType *)(ty));
    }
    return getRandomValue(ty);
  }
  return nullptr;
}

llvm::Value *FunctionMutant::getRandomValue(llvm::Type *ty) {
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

llvm::Value *FunctionMutant::getRandomFromGlobal(llvm::Type *ty) {
  if (ty != nullptr && !globals.empty()) {
    return LLVMUtil::findRandomInArray<llvm::Value *, llvm::Type *>(
        extraValues, ty,
        [](llvm::Value *v, llvm::Type *ty) { return v->getType() == ty; },
        nullptr);
  }
  return nullptr;
}
bool ComplexMutator::init() {
  for (auto fit = pm->begin(); fit != pm->end(); ++fit) {
    for (auto ait = fit->arg_begin(); ait != fit->arg_end(); ++ait) {
      if (ait->hasAttribute(llvm::Attribute::AttrKind::ImmArg)) {
        filterSet.insert(fit->getName());
      }
    }
  }
  for (auto fit = pm->begin(); fit != pm->end(); ++fit) {
    if (!fit->isDeclaration() && !fit->getName().empty() &&
        !invalidFunctions.contains(fit->getName())) {
      if (FunctionMutant::canMutate(&*fit, filterSet)) {
        functionMutants.push_back(
            std::make_shared<FunctionMutant>(&*fit, vMap, filterSet, globals));
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
      if (llvm::isa<ConstantInt>(*git)) {
        Random::addUsedInt(
            llvm::cast<llvm::ConstantInt>(*git).getLimitedValue());
      }
    }
    return true;
  }
  return false;
}

void ComplexMutator::resetTmpModule() {
  vMap.clear();
  tmpCopy = llvm::CloneModule(*pm, vMap);
  for (size_t i = 0; i < functionMutants.size(); ++i) {
    functionMutants[i]->resetTmpCopy(tmpCopy);
  }
}

void ComplexMutator::mutateModule(const std::string &outputFileName) {
  resetTmpModule();
  assert(curFunction < functionMutants.size() &&
         "curFunction should be a valid function");
  if (debug) {
    functionMutants[curFunction]->debug();
  }
  functionMutants[curFunction]->mutate();

  if (debug) {
    functionMutants[curFunction]->debug();
  }
  ++curFunction;
  if (curFunction == functionMutants.size()) {
    curFunction = 0;
  }
}

void ComplexMutator::saveModule(const std::string &outputFileName) {
  std::error_code ec;
  llvm::raw_fd_ostream fout(outputFileName, ec);
  fout << *tmpCopy;
  fout.close();
  if (debug) {
    llvm::errs() << "file wrote to " << outputFileName << "\n";
  }
}
