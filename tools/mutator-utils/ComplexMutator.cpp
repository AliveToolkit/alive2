#include "ComplexMutator.h"

void FunctionMutant::resetIterator(){
    bit = currentFunction->begin();
    iit = bit->begin();  
    initAtFunctionEntry();
}

bool FunctionMutant::canMutate(
    const llvm::Instruction& inst, const llvm::StringSet<> &filterSet) {
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

bool FunctionMutant::canMutate(
    const llvm::BasicBlock& block, const llvm::StringSet<> &filterSet) {
  return !block.getInstList().empty() &&
         std::all_of(block.begin(), block.end(),
                       [&filterSet](const llvm::Instruction& inst) {
                         return canMutate(inst, filterSet);
                       });
}

bool FunctionMutant::canMutate(
    const llvm::Function *function,
    const  llvm::StringSet<>&filterSet) {
  return !function->getBasicBlockList().empty() &&
         std::all_of(function->begin(), function->end(),
                       [&filterSet](const llvm::BasicBlock &bb) {
                         return canMutate(bb, filterSet);
                       });
}

void FunctionMutant::mutate() {
  
}

void FunctionMutant::moveToNextInstruction(){
  assert(domVals.inBackup());
  domVals.push_back(&*iit);
  ++iit;
  if (iit == bit->end()) {
    moveToNextBasicBlock();
  }
}

void FunctionMutant::initAtNewBasicBlock(){
  for (size_t i : whenMoveToNextBasicBlockFuncs) {
    helpers[i]->whenMoveToNextBasicBlock();
  }
}

void FunctionMutant::initAtNewInstruction(){
  for (size_t i : whenMoveToNextInstFuncs) {
    helpers[i]->whenMoveToNextInst();
  }
}

void FunctionMutant::initAtFunctionEntry(){
  for (size_t i : whenMoveToNextFuncFuncs) {
    helpers[i]->whenMoveToNextFunction();
  }
}


void FunctionMutant::moveToNextBasicBlock(){
  ++bit;
  if (bit == currentFunction->end()) {
    resetIterator();
  } else {
    iit = bit->begin();
  }
  calcDomVals();
  initAtNewBasicBlock();
}

void FunctionMutant::moveToNextMutant(){
  moveToNextInstruction();
  while (!canMutate(*iit,filterSet))
    moveToNextInstruction();
  domVals.restoreBackup();

}

void FunctionMutant::calcDomVals(){
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


void FunctionMutant::resetTmpCopy(std::shared_ptr<llvm::Module*> copy){
  extraValues.clear();
  tmpCopy = copy;
  functionInTmp = &*llvm::Module::iterator((llvm::Function *)&*vMap[currentFunction]);
  bitInTmp = llvm::Function::iterator((llvm::BasicBlock *)&*vMap[&*bit]);
  iitInTmp = llvm::BasicBlock::iterator((llvm::Instruction *)&*vMap[&*iit]);  
}

void FunctionMutant::setOperandRandomValue(llvm::Instruction *inst, size_t pos){
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

void FunctionMutant::addFunctionArguments(const llvm::SmallVector<llvm::Type *> &tys,
                          llvm::ValueToValueMapTy &VMap){
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

void FunctionMutant::fixAllValues(llvm::SmallVector<llvm::Value *> &vals){
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

llvm::Value *FunctionMutant::getRandomConstant(llvm::Type *ty){
  if (ty->isIntegerTy()) {
    return llvm::ConstantInt::get(ty, Random::getRandomUnsigned());
  }
  return llvm::UndefValue::get(ty);
}

llvm::Value *FunctionMutant::getRandomDominatedValue(llvm::Type *ty){
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

llvm::Value *FunctionMutant::getRandomValueFromExtraValue(llvm::Type *ty){
  if (ty != nullptr && !extraValues.empty()) {
    return LLVMUtil::findRandomInArray<llvm::Value*, llvm::Type*>(extraValues,ty,
      [](llvm::Value* v, llvm::Type* ty){return v->getType()==ty;},nullptr);
  }
  return nullptr;  
}

llvm::Value *FunctionMutant::getRandomPointerValue(llvm::Type *ty){ 
  if (ty->isPointerTy()) {
    if (Random::getRandomUnsigned() % (1 + valueFuncs.size()) == 0) {
      return llvm::ConstantPointerNull::get((llvm::PointerType *)(ty));
    }
    return getRandomValue(ty);
  }
  return nullptr;
}

llvm::Value* FunctionMutant::getRandomValue(llvm::Type* ty){
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

llvm::Value* FunctionMutant::getRandomFromGlobal(llvm::Type* ty){
  if(ty!=nullptr&&!globals.empty()){
    return LLVMUtil::findRandomInArray<llvm::Value*, llvm::Type*>(extraValues,ty,
      [](llvm::Value* v, llvm::Type* ty){return v->getType()==ty;},nullptr);
  }
  return nullptr;
}
bool ComplexMutator::init() {
  bool result = false;
  for (fit = pm->begin(); fit != pm->end(); ++fit) {
    if (fit->isDeclaration() || fit->getName().empty() ||
        invalidFunctions.find(fit->getName().str()) != invalidFunctions.end()) {
      continue;
    }
    for (bit = fit->begin(); bit != fit->end(); ++bit) {
      for (iit = bit->begin(); iit != bit->end(); ++iit) {
        if (isReplaceable(&*iit)) {
          result = true;
          goto end;
        }
      }
    }
  }

end:
  if (result) {
    for (auto funcIt = pm->begin(); funcIt != pm->end(); ++funcIt) {
      for (auto ait = funcIt->arg_begin(); ait != funcIt->arg_end(); ++ait) {
        if (ait->hasAttribute(llvm::Attribute::AttrKind::ImmArg)) {
          filterSet.insert(funcIt->getName().str());
          break;
        }
      }

      if (!funcIt->isDeclaration() && !funcIt->getName().empty() &&
          invalidFunctions.find(fit->getName().str()) ==
              invalidFunctions.end()) {
        /*
            Handle Dominator tree
        */
        dtMap[funcIt->getName()] = llvm::DominatorTree(*funcIt);
      }

      /*
        find all used ints and add them to the Random class
      */
      for (auto instIt = llvm::inst_begin(*funcIt);
           instIt != llvm::inst_end(*funcIt); instIt++) {
        for (size_t i = 0; i < instIt->getNumOperands(); ++i) {
          if (llvm::Value *val = instIt->getOperand(i);
              val != nullptr && llvm::isa<llvm::ConstantInt>(val)) {
            Random::addUsedInt(
                ((llvm::ConstantInt *)val)->getValue().getLimitedValue());
          }
        }
      }
    }

    for (auto git = pm->global_begin(); git != pm->global_end(); ++git) {
      domInst.push_back(&*git);

      for (auto ait = git->op_begin(); ait != git->op_end(); ++ait) {
        if (ait->get() != nullptr && llvm::isa<llvm::ConstantInt>(ait->get())) {
          llvm::ConstantInt *pci = (llvm::ConstantInt *)ait->get();
          Random::addUsedInt(pci->getValue().getLimitedValue());
        }
      }
      if (llvm::isa<llvm::ConstantInt>(&*git)) {
        llvm::ConstantInt *pci = (llvm::ConstantInt *)&*git;
        Random::addUsedInt(pci->getValue().getLimitedValue());
      }
    }
<<<<<<< HEAD
    return result;
}
=======
    calcDomInst();
    /*
      Hard code for when to update helpers
    */
    helpers.push_back(std::make_unique<ShuffleHelper>(this));
    whenMoveToNextFuncFuncs.push_back(helpers.size() - 1);
    whenMoveToNextBasicBlockFuncs.push_back(helpers.size() - 1);

    helpers.push_back(std::make_unique<RandomMoveHelper>(this));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

    helpers.push_back(std::make_unique<MutateInstructionHelper>(this));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

    helpers.push_back(std::make_unique<RandomCodeInserterHelper>(this));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

    helpers.push_back(std::make_unique<FunctionCallInlineHelper>(this));
    whenMoveToNextInstFuncs.push_back(helpers.size() - 1);

    for (size_t i = 0; i < helpers.size(); ++i) {
      helpers[i]->init();
    }
  }
>>>>>>> clang format

  return result;
}

void ComplexMutator::resetTmpModule() {
  vMap.clear();
  extraValue.clear();
  tmpCopy = llvm::CloneModule(*pm, vMap);
  tmpFit = llvm::Module::iterator((llvm::Function *)&*vMap[&*fit]);
  tmpBit = llvm::Function::iterator((llvm::BasicBlock *)&*vMap[&*bit]);
  tmpIit = llvm::BasicBlock::iterator((llvm::Instruction *)&*vMap[&*iit]);
}

void ComplexMutator::mutateModule(const std::string &outputFileName) {
  resetTmpModule();
  if (debug) {
    llvm::errs() << "Current function " << tmpFit->getName() << "\n";
    llvm::errs() << "Current basic block:\n";
    tmpBit->print(llvm::errs());
    llvm::errs() << "\nCurrent instruction:\n";
    tmpIit->print(llvm::errs());
    llvm::errs() << "\n";
  }
  currFuncName = tmpFit->getName().str();
  llvm::SmallVector<size_t> idxs;
  for (size_t idx = 0; idx < helpers.size(); ++idx) {
    if (helpers[idx]->shouldMutate()) {
      idxs.push_back(idx);
    }
  }
  if (!idxs.empty()) {
    size_t idx = Random::getRandomUnsigned() % idxs.size();
    helpers[idxs[idx]]->mutate();
    if (debug) {
      helpers[idxs[idx]]->debug();
    }
    for (; idx < idxs.size(); ++idx) {
      while (helpers[idxs[idx]]->shouldMutate() && Random::getRandomBool()) {
        helpers[idxs[idx]]->mutate();
        if (debug) {
          helpers[idxs[idx]]->debug();
        }
      }
    }
  }
  if (debug) {
    tmpBit->print(llvm::errs());
    llvm::errs() << "\nDT info"
                 << dtMap.find(fit->getName())
                        ->second.dominates(
                            &*(fit->getFunction().begin()->begin()), &*iit);
    llvm::errs() << "\n";
  }
  moveToNextReplaceableInst();
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

bool ComplexMutator::isReplaceable(llvm::Instruction *inst) {
  // contain immarg attributes
  if (llvm::isa<llvm::CallBase>(inst)) {
    // in case of cannot find function name
    if (llvm::Function *func = ((llvm::CallBase *)inst)->getCalledFunction();
        func != nullptr &&
        (filterSet.find(func->getName().str()) != filterSet.end() ||
         func->getName().startswith("llvm"))) {
      return false;
    }
<<<<<<< HEAD
    //don't do replacement on PHI node
    //don't update an alloca inst
    //don't do operations on Switch inst for now.
    if(llvm::isa<llvm::PHINode>(inst)||llvm::isa<llvm::GetElementPtrInst>(inst)||llvm::isa<llvm::AllocaInst>(inst)
        ||llvm::isa<llvm::SwitchInst>(inst)){
        return false;
    }
=======
  }
  // don't do replacement on PHI node
  // don't update an alloca inst
  // don't do operations on Switch inst for now.
  if (llvm::isa<llvm::PHINode>(inst) ||
      llvm::isa<llvm::GetElementPtrInst>(inst) ||
      llvm::isa<llvm::AllocaInst>(inst) || llvm::isa<llvm::SwitchInst>(inst)) {

    return false;
  }
>>>>>>> clang format

  // only consider inst within an integer type
  for (llvm::Use &u : inst->operands()) {
    if (u.get()->getType()->isIntegerTy()) {
      return true;
    }
  }
  return true;
}

void ComplexMutator::moveToNextFuction() {
  ++fit;
  if (fit == pm->end())
    fit = pm->begin();
  while (fit->isDeclaration() ||
         invalidFunctions.find(fit->getName().str()) !=
             invalidFunctions.end() ||
         fit->getName().empty()) {
    ++fit;
    if (fit == pm->end())
      fit = pm->begin();
  }
  bit = fit->begin();
  iit = bit->begin();
  // shuffleBasicBlockIndex=0;
  for (size_t i : whenMoveToNextFuncFuncs) {
    helpers[i]->whenMoveToNextFunction();
  }
}

void ComplexMutator::moveToNextBasicBlock() {
  ++bit;
  if (bit == fit->end()) {
    moveToNextFuction();
  } else {
    iit = bit->begin();
  }
  calcDomInst();
  // shuffleBlockIndex=0;
  for (size_t i : whenMoveToNextBasicBlockFuncs) {
    helpers[i]->whenMoveToNextBasicBlock();
  }
}

void ComplexMutator::moveToNextInst() {
  assert(domInst.inBackup());
  domInst.push_back(&*iit);
  ++iit;
  if (iit == bit->end()) {
    moveToNextBasicBlock();
  }
}

void ComplexMutator::moveToNextReplaceableInst() {
  moveToNextInst();
  while (!isReplaceable(&*iit))
    moveToNextInst();
  domInst.restoreBackup();
  for (size_t i : whenMoveToNextInstFuncs) {
    helpers[i]->whenMoveToNextInst();
  }
}

void ComplexMutator::calcDomInst() {
  domInst.deleteBackup();
  domInst.resize(pm->global_size());
  if (auto it = dtMap.find(fit->getName()); it != dtMap.end()) {
    // add Parameters
    for (auto ait = fit->arg_begin(); ait != fit->arg_end(); ++ait) {
      domInst.push_back(&*ait);
    }
    llvm::DominatorTree &DT = it->second;
    // add BasicBlocks before bitTmp
    for (auto bitTmp = fit->begin(); bitTmp != bit; ++bitTmp) {
      if (DT.dominates(&*bitTmp, &*bit)) {
        for (auto iitTmp = bitTmp->begin(); iitTmp != bitTmp->end(); ++iitTmp) {
          domInst.push_back(&*iitTmp);
        }
      }
    }
    domInst.startBackup();
    // add Instructions before iitTmp
    for (auto iitTmp = bit->begin(); iitTmp != iit; ++iitTmp) {
      if (DT.dominates(&*iitTmp, &*iit)) {
        domInst.push_back(&*iitTmp);
      }
    }
  }
}

llvm::Value *ComplexMutator::getRandomConstant(llvm::Type *ty) {
  if (ty->isIntegerTy()) {
    return llvm::ConstantInt::get(ty, Random::getRandomUnsigned());
  }
  return llvm::UndefValue::get(ty);
}

llvm::Value *ComplexMutator::getRandomDominatedValue(llvm::Type *ty) {
  if (ty != nullptr && !domInst.empty()) {
    for (size_t i = 0, pos = Random::getRandomUnsigned() % domInst.size();
         i < domInst.size(); ++i, ++pos) {
      if (pos == domInst.size())
        pos = 0;
      if (domInst[pos]->getType() == ty) {
        return &*vMap[domInst[pos]];
      }
    }
  }
  return nullptr;
}

llvm::Value *ComplexMutator::getRandomValueFromExtraValue(llvm::Type *ty) {
  if (ty != nullptr && !extraValue.empty()) {
    for (size_t i = 0, pos = Random::getRandomUnsigned() % extraValue.size();
         i < extraValue.size(); ++i, ++pos) {
      if (pos == extraValue.size())
        pos = 0;
      if (extraValue[pos]->getType() == ty) {
        return extraValue[pos];
      }
    }
  }
  return nullptr;
}

llvm::Value *ComplexMutator::getRandomValue(llvm::Type *ty) {
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

llvm::Value *ComplexMutator::getRandomPointerValue(llvm::Type *ty) {
  if (ty->isPointerTy()) {
    if (Random::getRandomUnsigned() % (1 + valueFuncs.size()) == 0) {
      return llvm::ConstantPointerNull::get((llvm::PointerType *)(ty));
    }
    return getRandomValue(ty);
  }
  return nullptr;
}

void ComplexMutator::addFunctionArguments(
    const llvm::SmallVector<llvm::Type *> &tys, llvm::ValueToValueMapTy &VMap) {
  if (!lazyUpdateInsts.empty()) {
    // llvm::SmallVector<llvm::Type*> tys;
    /*for(auto ait=tmpFit->arg_begin();ait!=tmpFit->arg_end();++ait){
        tys.push_back(ait->getType());
    }*/
    size_t oldArgSize = tmpFit->arg_size();
    LLVMUtil::insertFunctionArguments(&*tmpFit, tys, VMap);
    tmpIit = ((llvm::Instruction *)&*VMap[&*tmpIit])->getIterator();
    tmpBit = ((llvm::BasicBlock *)&*VMap[&*tmpBit])->getIterator();
    tmpFit = tmpBit->getParent()->getIterator();
    for (size_t i = 0; i < lazyUpdateInsts.size(); ++i) {
      lazyUpdateInsts[i] = (llvm::Instruction *)&*VMap[lazyUpdateInsts[i]];
    }

    for (auto it = vMap.begin(); it != vMap.end(); ++it) {
      if (VMap.find(it->second) != VMap.end()) {
        it->second = VMap[it->second];
      }
    }
    for (size_t i = 0; i < tys.size(); ++i) {
      extraValue.push_back(tmpFit->getArg(i + oldArgSize));
    }
  }
}

void ComplexMutator::fixAllValues(llvm::SmallVector<llvm::Value *> &vals) {
  if (!lazyUpdateInsts.empty()) {
    // llvm::errs()<<"extra values"<<extraValue.size()<<"CCCCCCC\n";
    llvm::ValueToValueMapTy VMap;
    addFunctionArguments(lazyUpdateArgTys, VMap);
    for (size_t i = 0; i < extraValue.size(); ++i) {
      if (VMap.find(extraValue[i]) != VMap.end()) {
        extraValue[i] = (llvm::Value *)&*VMap[extraValue[i]];
      }
    }
    for (size_t i = 0; i < vals.size(); ++i) {
      vals[i] = (llvm::Value *)&*VMap[vals[i]];
    }

    // llvm::errs()<<"extra values"<<extraValue.size()<<' '<<"CCCCCCC\n";
    // lazyUpdateInsts[0]->getParent()->print(llvm::errs());
    // llvm::errs()<<"\nextra values"<<extraValue.size()<<' '<<"CCCCCCC\n";
    // extraValue.back()->print(llvm::errs());
    // llvm::errs()<<"\nextra values"<<extraValue.size()<<' '<<"CCCCCCC\n";
    for (size_t i = 0; i < lazyUpdateInsts.size(); ++i) {
      lazyUpdateInsts[i]->setOperand(lazyUpdateArgPos[i],
                                     getRandomValue(lazyUpdateArgTys[i]));
      // llvm::errs()<<"after resetting value AAAAAAAAAAAAAAAAAAAAAAA\n";
      // lazyUpdateInsts[i]->print(llvm::errs());
      // llvm::errs()<<"\nAAAAAAAAAAAAAAAAAAAAAAA\n";
      // llvm::errs()<<lazyUpdateInsts[i]->getParent()->getParent()->getName()<<"\n";
    }
    lazyUpdateArgTys.clear();
    lazyUpdateArgPos.clear();
    lazyUpdateInsts.clear();
  }
}

void ComplexMutator::setOperandRandomValue(llvm::Instruction *inst,
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