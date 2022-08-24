#include "mutator_helper.h"
#include "mutator.h"

void ShuffleHelper::init() {
  shuffleUnitInBasicBlockIndex = 0;
  /**
   * find the same location as iit,bit,fit, and set shuffleBasicBlock
   *
   */
  for (auto bit = mutator->currentFunction->begin();
       bit != mutator->currentFunction->end();
       ++bit, ++shuffleUnitInBasicBlockIndex) {
    for (auto iit = bit->begin(); iit != bit->end(); ++iit) {
      if (&*iit == (&*(mutator->iit))) {
        goto varSetEnd;
      }
    }
  }
varSetEnd:
  llvm::Function *func = mutator->currentFunction;
  shuffleBlockInFunction.resize(func->getBasicBlockList().size());
  size_t idx = 0;
  for (auto bbIt = func->begin(); bbIt != func->end(); ++bbIt, ++idx) {
    ShuffleUnitInBasicBlock &bSBlock = shuffleBlockInFunction[idx];
    ShuffleUnit tmp;
    std::unordered_set<llvm::Value *> us;
    auto instIt = bbIt->begin();
    /**
     * handle phi instructions at beginning.
     */
    while (!instIt->isTerminator() && llvm::isa<llvm::PHINode>(&*instIt)) {
      tmp.push_back(&*instIt);
      ++instIt;
    }
    if (tmp.size() >= 2) {
      bSBlock.push_back(tmp);
    }
    tmp.clear();

    // pad inst has to be the first instruction in the block
    while (!instIt->isTerminator() &&
           mutator_util::isPadInstruction(&*instIt)) {
      ++instIt;
    }

    for (; !instIt->isTerminator(); ++instIt) {
      bool flag = true;
      for (size_t op = 0; flag && op < instIt->getNumOperands(); ++op) {
        if (us.find(instIt->getOperand(op)) != us.end()) {
          flag = false;
        }
      }
      if (!flag) {
        if (tmp.size() >= 2) {
          bSBlock.push_back(tmp);
        }
        tmp.clear();
        us.clear();
      }
      tmp.push_back(&*instIt);
      us.insert(&*instIt);
    }
    if (tmp.size() >= 2) {
      bSBlock.push_back(tmp);
    }
  }
}

bool ShuffleHelper::shouldMutate() {
  return shuffleBlockInFunction[shuffleUnitInBasicBlockIndex].size() >
         shuffleUnitIndex;
}

void ShuffleHelper::shuffleCurrentBlock() {
  ShuffleUnit &sblock =
      shuffleBlockInFunction[shuffleUnitInBasicBlockIndex][shuffleUnitIndex];
  llvm::SmallVector<llvm::Instruction *> sv;
  for (const auto &p : sblock) {
    sv.push_back(p);
  }
  int idx = mutator->domVals.find(sv[0]);
  llvm::Instruction *nextInst =
      (llvm::Instruction *)&*(mutator->vMap)[&*(++sv.back()->getIterator())];
  int findInSV = -1;
  for (int i = 0; i < (int)sv.size(); ++i) {
    if (sv[i]->getIterator() == mutator->iit) {
      findInSV = i;
      break;
    }
  }
  while (sv == sblock) {
    std::random_shuffle(sv.begin(), sv.end(),
                        [](int i) { return Random::getRandomUnsigned() % i; });
  }

  /**
   * 2 situations here.
   * the first is current iit is not in shuffle interval. Then shuffle interval
   * is either totally in domInst or totally not in domInst. the second is
   * current it is in shuffle interval. Then end of domInst must be pop first
   * and then insert those dom-ed insts.
   */
  if (findInSV == -1) {
    if (idx != -1) {
      for (size_t i = 0; i + idx < mutator->domVals.size() && i < sv.size();
           ++i) {
        mutator->domVals[i + idx] = sv[i];
      }
    }
  } else {
    while (findInSV--) {
      mutator->domVals.pop_back_tmp();
    }
    for (size_t i = 0; i < sv.size() && sv[i]->getIterator() != mutator->iit;
         ++i) {
      mutator->domVals.push_back_tmp(sv[i]);
    }
  }

  for (llvm::Instruction *p : sv) {
    ((llvm::Instruction *)&*(mutator->vMap)[p])->removeFromParent();
  }

  for (llvm::Instruction *p : sv) {
    ((llvm::Instruction *)&*(mutator->vMap)[p])->insertBefore(nextInst);
  }
  mutator->iitInTmp = llvm::BasicBlock::iterator(
      (llvm::Instruction *)&*mutator->vMap[&*mutator->iit]);
}

void ShuffleHelper::debug() {
  llvm::errs() << "\nInstructions shuffled\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
};

bool MutateInstructionHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); ++it) {
    if (canMutate(&*it)) {
      return true;
    }
  }
  return false;
}

bool MutateInstructionHelper::shouldMutate() {
  return !mutated && canMutate(&*mutator->iitInTmp);
}

void MutateInstructionHelper::debug() {
  if (!newAdded) {
    llvm::errs() << "\nReplaced with a existant usage\n";
  } else {
    llvm::errs() << "\nNew Inst added\n";
  }
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
}

void MutateInstructionHelper::mutate() {
  // do extra handling for br insts
  if (llvm::isa<llvm::BranchInst>(mutator->iitInTmp)) {
    /*llvm::BranchInst* brInst=(llvm::BranchInst*)&*mutator->tmpIit;
    unsigned sz=brInst->getNumSuccessors();
    llvm::SmallVector<llvm::BasicBlock*> bbs;
    if(sz>0){
      for(auto it=mutator->tmpFit->begin();it!=mutator->tmpFit->end();++it){
        bbs.push_back(&*it);
      }
    }
    for(unsigned i=0;i<sz;++i){
      if(Random::getRandomBool()){
        brInst->setSuccessor(i,bbs[Random::getRandomUnsigned()%bbs.size()]);
      }
    }*/
  }
  // 75% chances to add a new inst, 25% chances to replace with a existent usage
  else if ((Random::getRandomUnsigned() & 3) != 0) {
    bool res = insertRandomBinaryInstruction(&*(mutator->iitInTmp));
    if (!res) {
      replaceRandomUsage(&*(mutator->iitInTmp));
    }
    newAdded = res;
  } else {
    replaceRandomUsage(&*(mutator->iitInTmp));
  }
  mutated = true;
};

bool MutateInstructionHelper::insertRandomBinaryInstruction(
    llvm::Instruction *inst) {
  size_t pos = Random::getRandomUnsigned() % inst->getNumOperands();
  llvm::Type *ty = nullptr;
  for (size_t i = 0; i < inst->getNumOperands(); ++i, ++pos) {
    if (pos == inst->getNumOperands())
      pos = 0;
    if (inst->getOperand(pos)->getType()->isIntegerTy()) {
      ty = inst->getOperand(pos)->getType();
      break;
    }
  }

  if (ty == nullptr) {
    return false;
  }

  if (!ty->isFloatingPointTy() && !ty->isIntegerTy()) {
    return false;
  }

  bool isFloat = ty->isFloatingPointTy();

  llvm::Value *val1 = mutator->getRandomValue(ty),
              *val2 = mutator->getRandomValue(ty);
  llvm::Instruction *newInst = nullptr;
  if (isFloat) {
    newInst = mutator_util::getRandomFloatInstruction(val1, val2, inst);
  } else {
    newInst = mutator_util::getRandomIntegerInstruction(val1, val2, inst);
  }
  inst->setOperand(pos, newInst);
  return true;
}

void MutateInstructionHelper::replaceRandomUsage(llvm::Instruction *inst) {
  bool found = false;
  size_t pos = Random::getRandomUnsigned() % inst->getNumOperands();
  // make sure at least one
  for (size_t i = 0; !found && i < mutator->iitInTmp->getNumOperands();
       i++, pos++) {
    if (pos == mutator->iitInTmp->getNumOperands()) {
      pos = 0;
    }
    if (canMutate(mutator->iitInTmp->getOperand(pos))) {
      found = true;
      break;
    }
  }
  assert(found && "at least should find a location which is not a basic block "
                  "and function!");
  /*llvm::Type *ty = nullptr;
  for (size_t i = 0; i < inst->getNumOperands(); ++i, ++pos) {
    if (pos == inst->getNumOperands())
      pos = 0;
    if (inst->getOperand(pos)->getType()->isIntegerTy()) {
      ty = inst->getOperand(pos)->getType();
      break;
    }
  }
  llvm::Value *val = mutator->getRandomValue(ty);
  inst->setOperand(pos, val);*/
  mutator->setOperandRandomValue(inst, pos);
  llvm::SmallVector<llvm::Value *> vals;
  mutator->fixAllValues(vals);
}

bool MutateInstructionHelper::canMutate(llvm::Instruction *inst) {
  return
      // make sure at least one
      std::any_of(inst->op_begin(), inst->op_end(),
                  [](llvm::Use &use) { return canMutate(use.get()); }) &&
      (inst->getNumOperands() - llvm::isa<CallBase>(*inst) > 0) &&
      !llvm::isa<llvm::LandingPadInst>(inst)
      // The ret value of CleanupRet Inst must be a CleanupPad, needs extra
      // check so ignore for now.
      && !llvm::isa<llvm::CleanupReturnInst>(inst)
      // all catch related inst require the value has to be label
      && !llvm::isa<llvm::CatchPadInst>(inst) &&
      !llvm::isa<llvm::CatchSwitchInst>(inst) &&
      !llvm::isa<llvm::CatchReturnInst>(inst);
}

bool RandomMoveHelper::shouldMutate() {
  return !moved && mutator->bitInTmp->size() > 2 &&
         !mutator->iitInTmp->isTerminator() &&
         !mutator_util::isPadInstruction(&*mutator->iitInTmp);
}

bool RandomMoveHelper::canMutate(llvm::Function *func) {
  for (auto bit = func->begin(); bit != func->end(); ++bit) {
    if (bit->size() > 2) {
      return true;
    }
  }
  return false;
}

void RandomMoveHelper::mutate() {
  randomMoveInstruction(&*(mutator->iitInTmp));
  moved = true;
  mutator->extraValues.clear();
}

void RandomMoveHelper::randomMoveInstruction(llvm::Instruction *inst) {
  if (inst->getNextNonDebugInstruction()->isTerminator()) {
    // if(Random::getRandomBool()){
    randomMoveInstructionForward(inst);
  } else if (inst->getIterator() == (inst->getParent()->begin())) {
    randomMoveInstructionBackward(inst);
  } else {
    if (Random::getRandomBool()) {
      randomMoveInstructionForward(inst);
    } else {
      randomMoveInstructionBackward(inst);
    }
  }
}

void RandomMoveHelper::randomMoveInstructionForward(llvm::Instruction *inst) {
  size_t pos = 0, newPos, beginPos = 0;
  auto beginIt = inst->getParent()->begin();
  for (auto it = inst->getParent()->begin(); &*it != inst; ++it, ++pos)
    ;
  /**
   * PHINode must be the first inst in the basic block.
   * pad inst must be the first inst in the block
   */
  if (!llvm::isa<llvm::PHINode>(inst)) {
    for (llvm::Instruction *phiInst = &*beginIt;
         llvm::isa<llvm::PHINode>(phiInst);
         phiInst = phiInst->getNextNonDebugInstruction()) {
      ++beginPos, ++beginIt;
    }
  }
  if (!mutator_util::isPadInstruction(inst)) {
    for (llvm::Instruction *padInst = &*beginIt;
         mutator_util::isPadInstruction(padInst);
         padInst = padInst->getNextNonDebugInstruction()) {
      ++beginPos, ++beginIt;
    }
  }
  /*
   *  Current inst cannot move forward because current inst is not PHI inst,
   *  and there are zero or more PHI inst(s) in front of current inst.
   */
  if (pos == beginPos) {
    return;
  }

  newPos = Random::getRandomUnsigned() % (pos - beginPos) + beginPos;
  // llvm::errs()<<pos<<' '<<beginPos<<' '<<newPos<<"AAAAAAAAAAAAAA\n";
  // llvm::errs()<<"both pos: "<<pos<<' '<<newPos<<"\n";
  llvm::SmallVector<llvm::Instruction *> v;
  llvm::Instruction *newPosInst = inst;
  llvm::BasicBlock::iterator newPosIt = newPosInst->getIterator();
  for (size_t i = pos; i != newPos; --i) {
    --newPosIt;
    v.push_back(&*newPosIt);
    // remove Insts in current basic block
    assert(mutator->domVals.tmp_size() != 0);
    mutator->domVals.pop_back_tmp();
  }
  newPosInst = &*newPosIt;

  inst->moveBefore(newPosInst);
  mutator->iitInTmp = inst->getIterator();

  for (size_t i = 0; i < inst->getNumOperands(); ++i) {
    if (llvm::Value *op = inst->getOperand(i);
        std::find(v.begin(), v.end(), op) != v.end()) {
      mutator->setOperandRandomValue(inst, i);
    }
  }
  llvm::SmallVector<llvm::Value *> vals;
  mutator->fixAllValues(vals);
  // restore domInst
}

void RandomMoveHelper::randomMoveInstructionBackward(llvm::Instruction *inst) {
  size_t pos = 0, newPos, endPos = 0;

  for (auto it = inst->getParent()->begin(); &*it != inst; ++it, ++pos)
    ;
  if (llvm::isa<llvm::PHINode>(inst)) {
    for (llvm::Instruction *phiInst = &*inst->getParent()->begin();
         llvm::isa<llvm::PHINode>(phiInst);
         phiInst = phiInst->getNextNonDebugInstruction()) {
      ++endPos;
    }
  }
  if (endPos == 0) {
    endPos = inst->getParent()->size() - 1;
  }
  /*
   * Current inst is a phi instruction and is the end of phi instruction block.
   */
  if (pos + 1 == endPos) {
    return;
  }

  newPos = Random::getRandomInt() % (endPos - pos) + 1 + pos;
  // need fix all insts used current inst in [pos,newPos]
  llvm::Instruction *newPosInst = inst;
  llvm::BasicBlock::iterator newPosIt = newPosInst->getIterator();
  //[0] keeps pointing to inst, [1] keeps pointing to iterator inst.
  llvm::SmallVector<llvm::Value *> extraVals;
  extraVals.push_back(inst);
  extraVals.push_back(newPosInst);
  for (size_t i = pos + 1; i != newPos; ++i) {
    ++newPosIt;
    newPosInst = &*newPosIt;
    extraVals[1] = newPosInst;
    for (size_t op = 0; op < newPosInst->getNumOperands(); ++op) {
      if (llvm::Value *opP = newPosInst->getOperand(op);
          opP != nullptr && opP == extraVals[0]) {
        mutator->setOperandRandomValue(newPosInst, op);
      }
    }
    // llvm::errs()<<"\nAAAAAAAAAAAAAA\n";
    // mutator->domInst.back()->print(llvm::errs());
    mutator->fixAllValues(extraVals);
    newPosInst = (llvm::Instruction *)extraVals[1];
    newPosIt = newPosInst->getIterator();
    mutator->extraValues.push_back(newPosInst);
  }
  inst = (llvm::Instruction *)extraVals[0];
  inst->moveBefore(newPosInst);
  mutator->iitInTmp = inst->getIterator();
}

void RandomMoveHelper::debug() {
  llvm::errs() << "Instruction was moved around\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
}

// don't allow insert code at LandPadInst.
// don't allow insert code at CatchPadInst.
// don't allow insert code at CleanupPadInst.
bool RandomCodeInserterHelper::shouldMutate() {
  llvm::Instruction *inst = &*mutator->iitInTmp;
  return !generated && !llvm::isa<llvm::PHINode>(inst) &&
         !mutator_util::isPadInstruction(inst);
}

void RandomCodeInserterHelper::debug() {
  llvm::errs() << "Code piece generated\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
}

void RandomCodeInserterHelper::mutate() {
  generated = true;
  // if not the first inst of this block, we can do a split
  llvm::Instruction *insertPoint = &*mutator->iitInTmp;
  if (mutator->bitInTmp->getFirstNonPHIOrDbg() != insertPoint) {
    llvm::BasicBlock *oldBB = &*mutator->bitInTmp;
    llvm::Instruction *inst = oldBB->getTerminator();
    llvm::SmallVector<llvm::BasicBlock *> succs;
    for (size_t i = 0; i < inst->getNumOperands(); ++i) {
      if (llvm::Value *val = inst->getOperand(i); llvm::isa<BasicBlock>(val)) {
        succs.push_back((llvm::BasicBlock *)val);
      }
    }
    llvm::BasicBlock *newBB =
        mutator->bitInTmp->splitBasicBlock(mutator->iitInTmp);
    for (auto bb : succs) {
      bb->replacePhiUsesWith(oldBB, newBB);
    }
    mutator->bitInTmp = newBB->getIterator();
  }
  mutator_util::insertRandomCodeBefore(insertPoint);
}

void FunctionCallInlineHelper::init() {
  if (funcToId.empty()) {
    llvm::Module *module = mutator->currentFunction->getParent();
    for (auto fit = module->begin(); fit != module->end(); ++fit) {
      if (fit->isDeclaration() || fit->getName().empty()) {
        continue;
      }
      bool shouldAdd = true;
      for (size_t i = 0; i < idToFuncSet.size() && shouldAdd; ++i) {
        if (mutator_util::compareSignature(
                &*fit, module->getFunction(idToFuncSet[i][0]))) {
          funcToId.insert(
              std::make_pair(fit->getName(), funcToId[idToFuncSet[i][0]]));
          idToFuncSet[i].push_back(fit->getName().str());
          shouldAdd = false;
        }
      }
      if (shouldAdd) {
        funcToId.insert(std::make_pair(fit->getName(), idToFuncSet.size()));
        idToFuncSet.push_back(std::vector<std::string>());
        idToFuncSet.back().push_back(fit->getName().str());
      }
    }
  }
  inlined = false;
}

/*
 * Not inlined.
 * is a function call
 * could find replacement
 */
bool FunctionCallInlineHelper::shouldMutate() {
  return !inlined && canMutate(&*mutator->iitInTmp);
}

llvm::Function *FunctionCallInlineHelper::getReplacedFunction() {
  assert(llvm::isa<llvm::CallInst>(mutator->iitInTmp) &&
         "function inline should be a call inst");
  llvm::CallInst *callInst = (llvm::CallInst *)&*mutator->iitInTmp;
  llvm::Function *func = callInst->getCalledFunction();
  functionInlined = func->getName();
  auto it = funcToId.find(func->getName());
  if (it != funcToId.end() && !idToFuncSet[it->second].empty()) {
    // make sure there is a replacement
    size_t idx = Random::getRandomUnsigned() % idToFuncSet[it->second].size();
    functionInlined = idToFuncSet[it->second][idx];
    // final check the inlined function must have the same type with called
    // function because of the pre-calculated function signature might be added
    // with more args
    if (mutator->tmpCopy->getFunction(functionInlined)->getFunctionType() !=
        func->getFunctionType()) {
      functionInlined = func->getName();
    }
  }
  return mutator->tmpCopy->getFunction(functionInlined);
}

bool FunctionCallInlineHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); it++) {
    if (canMutate(&*it)) {
      return true;
    }
  }
  return false;
}

void FunctionCallInlineHelper::mutate() {
  inlined = true;
  llvm::InlineFunctionInfo ifi;
  llvm::Function *func = getReplacedFunction();
  llvm::CallInst *callInst = (llvm::CallInst *)(&*mutator->iitInTmp);
  llvm::BasicBlock *block = callInst->getParent();
  llvm::BasicBlock::iterator backupIt =
      callInst->getIterator() == block->begin() ? block->end()
                                                : --callInst->getIterator();
  callInst->setCalledFunction(func);
  llvm::InlineResult res = llvm::InlineFunction(*callInst, ifi);
  if (!res.isSuccess()) {
    llvm::errs() << res.getFailureReason() << "\n";
  } else {
    if (backupIt == block->end()) {
      mutator->iitInTmp = block->begin();
    } else {
      mutator->iitInTmp = ++backupIt;
    }
  }
}

bool VoidFunctionCallRemoveHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); it++) {
    if (canMutate(&*it)) {
      return true;
    }
  }
  return false;
}

void VoidFunctionCallRemoveHelper::mutate() {
  assert(llvm::isa<CallInst>(&*mutator->iitInTmp) &&
         "the void call has to be a call inst to be removed");
  llvm::CallInst *callInst = (llvm::CallInst *)&*mutator->iitInTmp;
  llvm::Instruction *nextInst = callInst->getNextNonDebugInstruction();
  if (funcName.empty()) {
    funcName = callInst->getName().str();
  }
  callInst->eraseFromParent();
  removed = true;
  mutator->iitInTmp = nextInst->getIterator();
}

bool VoidFunctionCallRemoveHelper::shouldMutate() {
  return !removed && canMutate(&*mutator->iitInTmp);
}

void VoidFunctionCallRemoveHelper::debug() {
  if (removed) {
    llvm::errs() << "VoidFunctionCallRemoveHelper: Removed function\n"
                 << funcName;
    llvm::errs() << "\nBaisc block\n";
    mutator->iitInTmp->getParent()->print(llvm::errs());
    llvm::errs() << "\n";
  }
}

void FunctionAttributeHelper::init() {
  llvm::Function *func = mutator->currentFunction;
  size_t index = 0;
  for (auto ait = func->arg_begin(); ait != func->arg_end(); ait++, index++) {
    if (ait->getType()->isPointerTy()) {
      ptrPos.push_back(index);
    } else if (ait->getType()->isIntegerTy()) {
      llvm::IntegerType *intTy = (llvm::IntegerType *)ait->getType();
      size_t bitWidth = intTy->getBitWidth();
      if (disableEXT.find(bitWidth) != disableEXT.end()) {
        // do nothing
      } else if (disableSEXT.find(bitWidth) != disableSEXT.end()) {
        onlyZEXTPos.push_back(index);
      } else if (disableZEXT.find(bitWidth) != disableZEXT.end()) {
        onlySEXTPos.push_back(index);
      } else {
        bothEXTPos.push_back(index);
      }
    }
  }
}

#define setFuncAttr(attrName, value)                                           \
  if (func->hasFnAttribute(attrName)) {                                        \
    func->removeFnAttr(attrName);                                              \
  }                                                                            \
  if (value) {                                                                 \
    func->addFnAttr(attrName);                                                 \
  }
#define setFuncParamAttr(index, attrName, value)                               \
  if (func->hasParamAttribute(index, attrName)) {                              \
    func->removeParamAttr(index, attrName);                                    \
  }                                                                            \
  if (value) {                                                                 \
    func->addParamAttr(index, attrName);                                       \
  }

#define setFuncRetAttr(attrName, value)                                        \
  if (func->hasRetAttribute(attrName)) {                                       \
    func->removeRetAttr(attrName);                                             \
  }                                                                            \
  if (value) {                                                                 \
    func->addRetAttr(attrName);                                                \
  }

void FunctionAttributeHelper::mutate() {
  updated = true;
  llvm::Function *func = mutator->currentFunction;
  setFuncAttr(llvm::Attribute::AttrKind::NoFree, Random::getRandomBool());
  if (func->getReturnType()->isIntegerTy()) {
    llvm::IntegerType *intTy = (llvm::IntegerType *)func->getReturnType();
    size_t bitWidth = intTy->getBitWidth();
    if (disableEXT.find(bitWidth) == disableEXT.end()) {
      setFuncRetAttr(llvm::Attribute::AttrKind::ZExt, false);
      setFuncRetAttr(llvm::Attribute::AttrKind::SExt, false);
      if (disableZEXT.find(bitWidth) != disableZEXT.end()) {
        setFuncRetAttr(llvm::Attribute::AttrKind::SExt, Random::getRandomBool());
      } else if (disableSEXT.find(bitWidth) != disableSEXT.end()) {
        setFuncRetAttr(llvm::Attribute::AttrKind::ZExt, Random::getRandomBool());
      } else {
        setFuncRetAttr(Random::getRandomBool()
                           ? llvm::Attribute::AttrKind::ZExt
                           : llvm::Attribute::AttrKind::SExt,
                       Random::getRandomBool());
      }
    }
  }
  for (size_t index : ptrPos) {
    setFuncParamAttr(index, llvm::Attribute::AttrKind::NoCapture,
                     Random::getRandomBool());
    func->removeParamAttr(index, llvm::Attribute::AttrKind::Dereferenceable);
    func->addDereferenceableParamAttr(index,
                                      1 << (Random::getRandomUnsigned() % 4));
  }
  for (size_t index : onlySEXTPos) {
    setFuncParamAttr(index, llvm::Attribute::AttrKind::ZExt, false);
    setFuncParamAttr(index, llvm::Attribute::AttrKind::SExt,
                     Random::getRandomBool());
  }
  for (size_t index : onlySEXTPos) {
    setFuncParamAttr(index, llvm::Attribute::AttrKind::SExt, false);
    setFuncParamAttr(index, llvm::Attribute::AttrKind::ZExt,
                     Random::getRandomBool());
  }

  for (size_t index : bothEXTPos) {
    setFuncParamAttr(index, llvm::Attribute::AttrKind::ZExt, false);
    setFuncParamAttr(index, llvm::Attribute::AttrKind::SExt, false);
    setFuncParamAttr(index,
                     Random::getRandomBool() ? llvm::Attribute::AttrKind::ZExt
                                             : llvm::Attribute::AttrKind::SExt,
                     Random::getRandomBool());
  }
}

#undef setFuncAttr
#undef setFuncParamAttr
#undef setFuncRetAttr

void FunctionAttributeHelper::debug() {
  llvm::errs() << "FunctionAttributeHelper: Function attributes updated\n";
}

void GEPHelper::mutate() {
  llvm::GetElementPtrInst *inst =
      (llvm::GetElementPtrInst *)&*mutator->iitInTmp;
  inst->setIsInBounds(!inst->isInBounds());
  updated = true;
}

bool GEPHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); it++) {
    if (llvm::isa<llvm::GetElementPtrInst>(&*it)) {
      return true;
    }
  }
  return false;
}

bool GEPHelper::shouldMutate() {
  return !updated && llvm::isa<llvm::GetElementPtrInst>(&*mutator->iitInTmp);
}

void GEPHelper::debug() {
  llvm::errs() << "GEPHelper: Original GEP inst:\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "inbounds flag reversed.\n";
}

void BinaryInstructionHelper::resetFastMathFlags(llvm::BinaryOperator *inst) {
  if (llvm::isa<llvm::FPMathOperator>(inst)) {
    llvm::FastMathFlags flags;
    flags.setAllowContract(Random::getRandomBool());
    flags.setAllowReassoc(Random::getRandomBool());
    flags.setAllowReciprocal(Random::getRandomBool());
    flags.setApproxFunc(Random::getRandomBool());
    flags.setNoInfs(Random::getRandomBool());
    flags.setNoNaNs(Random::getRandomBool());
    flags.setNoSignedZeros(Random::getRandomBool());
    inst->setFastMathFlags(flags);
  }
}

void BinaryInstructionHelper::resetNUWNSWFlags(llvm::BinaryOperator *inst) {
  if (llvm::isa<llvm::OverflowingBinaryOperator>(inst)) {
    inst->setHasNoSignedWrap(Random::getRandomBool());
    inst->setHasNoUnsignedWrap(Random::getRandomBool());
  }
}

void BinaryInstructionHelper::resetExactFlag(llvm::BinaryOperator *inst) {
  inst->setIsExact(Random::getRandomBool());
}

const std::vector<std::function<void(llvm::BinaryOperator *)>>
    BinaryInstructionHelper::flagFunctions(
        {BinaryInstructionHelper::doNothing,
         BinaryInstructionHelper::resetNUWNSWFlags,
         BinaryInstructionHelper::resetFastMathFlags,
         BinaryInstructionHelper::resetExactFlag,
         BinaryInstructionHelper::resetNUWNSWFlags,
         BinaryInstructionHelper::resetExactFlag,
         BinaryInstructionHelper::doNothing});

#define Ops llvm::Instruction::BinaryOps
const std::unordered_map<llvm::Instruction::BinaryOps, int>
    BinaryInstructionHelper::operToIndex({{Ops::URem, 0},
                                          {Ops::SRem, 0},
                                          {Ops::Add, 1},
                                          {Ops::Sub, 1},
                                          {Ops::Mul, 1},
                                          {Ops::FAdd, 2},
                                          {Ops::FSub, 2},
                                          {Ops::FMul, 2},
                                          {Ops::FDiv, 2},
                                          {Ops::FRem, 2},
                                          {Ops::UDiv, 3},
                                          {Ops::SDiv, 3},
                                          {Ops::Shl, 4},
                                          {Ops::LShr, 5},
                                          {Ops::AShr, 5},
                                          {Ops::And, 6},
                                          {Ops::Or, 6},
                                          {Ops::Xor, 6}});

const std::vector<std::vector<llvm::Instruction::BinaryOps>>
    BinaryInstructionHelper::indexToOperSet({{
                                                 Ops::URem,
                                                 Ops::SRem,
                                             },
                                             {Ops::Add, Ops::Sub, Ops::Mul},
                                             {Ops::FAdd, Ops::FSub, Ops::FMul,
                                              Ops::FDiv, Ops::FRem},
                                             {Ops::UDiv, Ops::SDiv},
                                             {Ops::Shl},
                                             {Ops::LShr, Ops::AShr},
                                             {Ops::And, Ops::Or, Ops::Xor}});
#undef attr

void BinaryInstructionHelper::mutate() {
  llvm::BinaryOperator *binInst = (llvm::BinaryOperator *)(&*mutator->iitInTmp);
  if (Random::getRandomBool()) {
    swapOperands(binInst);
  }
  int opIndex = getOpIndex(binInst);
  llvm::Instruction::BinaryOps op = getNewOperator(opIndex);
  llvm::BinaryOperator *newInst = llvm::BinaryOperator::Create(
      op, binInst->getOperand(0), binInst->getOperand(1), "", binInst);
  resetMathFlags(newInst, opIndex);
  binInst->replaceAllUsesWith(newInst);
  binInst->eraseFromParent();
  mutator->iitInTmp = newInst->getIterator();
}

bool BinaryInstructionHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); ++it) {
    if (llvm::isa<llvm::BinaryOperator>(&*it)) {
      return true;
    }
  }
  return false;
}

bool BinaryInstructionHelper::shouldMutate() {
  return !updated && llvm::isa<llvm::BinaryOperator>(&*mutator->iitInTmp);
}

void BinaryInstructionHelper::debug() {
  llvm::errs() << "\nBinaryInstructionHelper: Current binary inst:\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
  mutator->iitInTmp->getParent()->print(llvm::errs());
}

bool ResizeIntegerHelper::isValidNode(llvm::Value *val) {
  if (llvm::isa<llvm::BinaryOperator>(val)) {
    return llvm::isa<llvm::IntegerType>(val->getType());
  }
  return false;
}

bool ResizeIntegerHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); ++it) {
    if (isValidNode(&*it)) {
      return true;
    }
  }
  return false;
}

bool ResizeIntegerHelper::shouldMutate() {
  return !updated && isValidNode(&*mutator->iitInTmp);
}

// 1, 8, 16, 32, 64 50%
// 1....64 50%
llvm::IntegerType *
ResizeIntegerHelper::getNewIntegerTy(llvm::LLVMContext &context) {
  static llvm::SmallVector<size_t> defaultWidth{1, 8, 16, 32, 64};
  if (Random::getRandomBool()) {
    return llvm::IntegerType::get(
        context,
        defaultWidth[Random::getRandomUnsigned() % defaultWidth.size()]);
  } else {
    return llvm::IntegerType::get(context,
                                  1 + Random::getRandomUnsigned() % 64);
  }
}

std::vector<llvm::Instruction *>
ResizeIntegerHelper::constructUseChain(llvm::Instruction *startPoint) {
  std::vector<llvm::Instruction *> res;
  llvm::Instruction *cur = startPoint;
  bool hasNext = false;
  do {
    hasNext = false;
    if (!cur->use_empty()) {
      size_t i = 0;
      auto use_it = cur->use_begin();
      // reset use_it at random pos
      for (size_t tmp = Random::getRandomUnsigned() % cur->getNumUses();
           tmp != 0; --tmp, ++use_it)
        ;
      // reset end

      for (; i < cur->getNumUses(); ++use_it, ++i) {
        if (use_it == cur->use_end()) {
          use_it = cur->use_begin();
        }
        llvm::Value *val = use_it->getUser();
        if (isValidNode(val) &&
            std::find(res.begin(), res.end(), val) == res.end()) {
          hasNext = true;
          res.push_back(cur);
          cur = (llvm::Instruction *)val;
          break;
        }
      }
    }
  } while (hasNext);
  res.push_back(cur);
  return res;
}

llvm::Instruction *
ResizeIntegerHelper::updateNode(llvm::Instruction *val,
                                llvm::ArrayRef<llvm::Value *> args) {
  assert(args.size() == 2);
  if (llvm::isa<llvm::BinaryOperator>(val)) {
    llvm::BinaryOperator *op = (llvm::BinaryOperator *)val;
    llvm::Instruction *nextInst = op->getNextNonDebugInstruction();
    llvm::BinaryOperator *newOp = llvm::BinaryOperator::Create(
        op->getOpcode(), args[0], args[1], "", nextInst);
    assert(newOp->getType()->isIntegerTy());
    llvm::IntegerType *newIntTy = (llvm::IntegerType *)newOp->getType();
    for (size_t i = 0; i < newOp->getNumOperands(); ++i) {
      if (llvm::isa<llvm::UndefValue>(newOp->getOperand(i))) {
        llvm::Value *resizedVal =
            mutator_util::updateIntegerSize(op->getOperand(i), newIntTy, newOp);
        newOp->setOperand(i, resizedVal);
      }
    }
    return newOp;
  }
  return nullptr;
}

void ResizeIntegerHelper::updateChain(std::vector<llvm::Instruction *> &chain,
                                      llvm::IntegerType *newIntTy) {
  std::vector<llvm::Instruction *> newChain;
  if (!chain.empty()) {
    llvm::Value *undef = llvm::UndefValue::get(newIntTy);
    llvm::SmallVector<llvm::Value *> args;
    // head
    args.push_back(undef);
    args.push_back(undef);
    llvm::Instruction *headInst = updateNode(chain.front(), args);
    chain.front()->setName("old0");
    headInst->setName("new" + std::to_string(newChain.size()));
    newChain.push_back(headInst);
    args.clear();
    // mid + tail
    for (size_t i = 1; i < chain.size(); ++i) {
      for (size_t pos = 0; pos < chain[i]->getNumOperands(); ++pos) {
        if (chain[i]->getOperand(pos) == chain[i - 1]) {
          args.push_back(newChain[i - 1]);
        } else {
          args.push_back(undef);
        }
      }
      llvm::Instruction *inst = updateNode(chain[i], args);
      inst->setName("new" + std::to_string(newChain.size()));
      chain[i]->setName("old" + std::to_string(newChain.size()));
      newChain.push_back(inst);
      args.clear();
    }
    llvm::Instruction *nextInst = newChain.back()->getNextNonDebugInstruction();
    llvm::Value *extBack = mutator_util::updateIntegerSize(
        newChain.back(), (llvm::IntegerType *)chain.back()->getType(),
        nextInst);
    extBack->setName("last");
    chain.back()->replaceAllUsesWith(extBack);
  }
}

void ResizeIntegerHelper::resizeOperand(llvm::Instruction *inst, size_t index,
                                        llvm::IntegerType *newTy) {
  assert(inst->getNumOperands() > index);
  llvm::Value *val = inst->getOperand(index);
  assert(llvm::isa<llvm::IntegerType>(val->getType()));

  llvm::Value *newOp = mutator_util::updateIntegerSize(val, newTy, inst);
  inst->setOperand(index, newOp);
}

void ResizeIntegerHelper::mutate() {
  llvm::Instruction *inst = &*mutator->iitInTmp;
  std::vector<llvm::Instruction *> useChain = constructUseChain(inst);
  llvm::IntegerType *oldIntTy = (llvm::IntegerType *)inst->getType(),
                    *newIntTy = oldIntTy;
  do {
    newIntTy = getNewIntegerTy(inst->getContext());
  } while (newIntTy == oldIntTy);
  updateChain(useChain, newIntTy);
  updated = true;
}

void ResizeIntegerHelper::debug() {
  llvm::errs() << "integer resized\n";
  mutator->iitInTmp->getParent()->print(llvm::errs());
  llvm::errs() << "\n";
}