#include "mutator_helper.h"
#include "mutator.h"

void ShuffleHelper::init() {
  llvm::Function *func = mutator->currentFunction;
  shuffleBlockInFunction.init(func->size());
  for (auto bbIt = func->begin(); bbIt != func->end(); ++bbIt) {
    shuffleBlockInFunction[&*bbIt] = ShuffleUnitInBasicBlock();
    ShuffleUnitInBasicBlock &bSBlock = shuffleBlockInFunction[&*bbIt];
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
  return shuffleBlockInFunction[&*mutator->bit].size() > shuffleUnitIndex;
}

void ShuffleHelper::shuffleCurrentBlock() {
  ShuffleUnit &sblock =
      shuffleBlockInFunction[&*mutator->bit][shuffleUnitIndex];
  llvm::SmallVector<llvm::Instruction *> sv;
  for (const auto &p : sblock) {
    sv.push_back(p);
  }
  llvm::Instruction *nextInst =
      (llvm::Instruction *)&*(mutator->vMap)[&*(++sv.back()->getIterator())];
  bool findInSV = false;
  for (size_t i = 0; i < sv.size(); ++i) {
    if (sv[i]->getIterator() == mutator->iit) {
      findInSV = true;
      break;
    }
  }
  while (sv == sblock) {
    std::shuffle(sv.begin(), sv.end(), Random::getRNG());
  }

  /**
   * 2 situations here.
   * the first is current iit is not in shuffle interval. Then shuffle interval
   * is either totally in domInst or totally not in domInst. the second is
   * current it is in shuffle interval. Then end of domInst must be pop first
   * and then insert those dom-ed insts.
   */
  if (findInSV) {
    for (size_t i = 0; i < sv.size(); ++i) {
      mutator->invalidValues.insert(
          (llvm::Instruction *)&*mutator->vMap[sv[i]]);
    }
    for (size_t i = 0; i < sv.size() && sv[i]->getIterator() != mutator->iit;
         ++i) {
      llvm::Instruction *mappedVal =
          (llvm::Instruction *)&*mutator->vMap[sv[i]];
      if (mutator->invalidValues.contains(mappedVal)) {
        mutator->invalidValues.erase(mappedVal);
      }
      mutator->extraValues.push_back(mappedVal);
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
  mutator->bitInTmp->print(llvm::errs());
  for (auto it = mutator->invalidValues.begin();
       it != mutator->invalidValues.end(); ++it) {
    (*it)->dump();
  }
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
    // empty for now
  }
  // 75% chances to add a new inst, 25% chances to replace with a existent usage
  else if ((Random::getRandomUnsigned() & 3) != 0) {
    // indices in GEP point to its member variabls, shouldn't be random changed.
    bool isGEPInst = llvm::isa<llvm::GetElementPtrInst>(*mutator->iitInTmp),
         res = false;
    if (!isGEPInst) {
      res = insertRandomBinaryInstruction(&*(mutator->iitInTmp));
    }
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
    if (inst->getOperand(pos)->getType()->isIntOrIntVectorTy() ||
        inst->getOperand(pos)->getType()->isFPOrFPVectorTy()) {
      ty = inst->getOperand(pos)->getType();
      break;
    }
  }

  if (ty == nullptr) {
    return false;
  }

  if (!ty->isIntOrIntVectorTy() && !ty->isFPOrFPVectorTy()) {
    return false;
  }

  /*
   * ExtractElement
   * InsertElement
   * ShuffleElement
   */

  bool isFloat = ty->isFPOrFPVectorTy();
  llvm::Instruction *newInst = nullptr;

  /*
   * handleVector case;
   */
  if (Random::getRandomUnsigned() % 4 == 0 && ty->isVectorTy()) {
    size_t choice = Random::getRandomUnsigned() % 2;
    llvm::Type *eleType = nullptr;
    llvm::IntegerType *i32Ty = llvm::Type::getInt32Ty(inst->getContext());
    if (choice == 0) {
      size_t idx = 0;
      if (llvm::isa<llvm::FixedVectorType>(ty)) {
        llvm::FixedVectorType *fixedVectorType = (llvm::FixedVectorType *)ty;
        idx = Random::getRandomUnsigned() % fixedVectorType->getNumElements();
        eleType = fixedVectorType->getElementType();
      } else {
        llvm::ScalableVectorType *scalableVectorType =
            (llvm::ScalableVectorType *)ty;
        idx = Random::getRandomUnsigned() %
              scalableVectorType->getMinNumElements();
        eleType = scalableVectorType->getElementType();
      }
      llvm::Value *val1 = mutator->getRandomValue(ty),
                  *val2 = llvm::UndefValue::get(eleType),
                  *val3 = llvm::ConstantInt::get(i32Ty, idx);
      newInst = llvm::InsertElementInst::Create(val1, val2, val3);
      newInst->insertBefore(inst);
      inst->setOperand(pos, newInst);
      mutator->setOperandRandomValue(newInst, 1);
      llvm::SmallVector<llvm::Value *> vals;
      mutator->fixAllValues(vals);
    } else if (choice == 1) {
      llvm::Value *val1 = mutator->getRandomValue(ty),
                  *val2 = mutator->getRandomValue(ty);
      llvm::Value *val3 = nullptr;
      if (llvm::isa<llvm::FixedVectorType>(ty)) {
        llvm::FixedVectorType *fixedVectorType = (llvm::FixedVectorType *)ty;
        size_t nums = fixedVectorType->getNumElements();
        size_t nums2 = nums << 1;
        llvm::SmallVector<llvm::Constant *> vals;
        for (size_t i = 0; i < nums; ++i) {
          vals.push_back(llvm::ConstantInt::get(
              i32Ty, Random::getRandomUnsigned() % nums2));
        }
        val3 = llvm::ConstantVector::get(vals);
      } else {
        val3 = llvm::Constant::getNullValue(ty);
      }
      assert(val3 != nullptr);
      // llvm::ShuffleVectorInst shuffleVectorInst(val1, val2, val3);
      // shuffleVectorInst.insertBefore(inst);
      // inst->setOperand(pos, &shuffleVectorInst);
      newInst = new llvm::ShuffleVectorInst(val1, val2, val3);
      newInst->insertBefore(inst);
    }

  } else {
    llvm::Value *val1 = mutator->getRandomValue(ty),
                *val2 = mutator->getRandomValue(ty);

    if (isFloat) {
      newInst = mutator_util::getRandomFloatInstruction(val1, val2, inst);
    } else {
      newInst = mutator_util::getRandomIntegerInstruction(val1, val2, inst);
    }
  }
  return true;
}

void MutateInstructionHelper::replaceRandomUsage(llvm::Instruction *inst) {
  bool found = false;
  size_t pos = Random::getRandomUnsigned() % inst->getNumOperands();
  bool isGEP = llvm::isa<llvm::GetElementPtrInst>(inst);
  // make sure at least one
  for (size_t i = 0; !found && i < mutator->iitInTmp->getNumOperands();
       i++, pos++) {
    if (pos == mutator->iitInTmp->getNumOperands()) {
      pos = 0;
    }
    if (canMutate(mutator->iitInTmp->getOperand(pos))) {
      if (isGEP &&
          llvm::isa<llvm::Constant>(mutator->iitInTmp->getOperand(pos))) {
        continue;
      }
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
  // skip those call base with inlning asm.
  if (llvm::isa<llvm::CallBase>(inst)) {
    llvm::CallBase *callBase = (llvm::CallBase *)inst;
    // don't update on call with kcfi, they are required to be constants.
    if (callBase->getOperandBundle("kcfi").has_value()) {
      return false;
    }
    if (callBase->isInlineAsm()) {
      return false;
    }
  }

  if (llvm::isa<llvm::GetElementPtrInst>(inst)) {
    for (auto it = inst->op_begin(); it != inst->op_end(); ++it) {
      if (!canMutate(it->get()) && !llvm::isa<llvm::Constant>(it->get())) {
        return true;
      }
    }
    return false;
  }

  return
      // make sure at least one
      std::any_of(inst->op_begin(), inst->op_end(),
                  [](llvm::Use &use) { return canMutate(use.get()); }) &&
      (inst->getNumOperands() - llvm::isa<llvm::CallBase>(*inst) > 0) &&
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
    mutator->invalidValues.insert(&*newPosIt);
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
  // newPos should be in [pos+2, endPos]
  // Because we use moveBefore, moving pos before pos + 1 makes no change
  newPos = Random::getRandomInt() % (endPos - pos - 1) + 2 + pos;
  // llvm::errs()<<"AAAAAAAAAAAAAA\n";
  // llvm::errs()<<pos<<" "<<newPos<<" "<<endPos<<"\n";
  //  need fix all insts used current inst in [pos,newPos]
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
  // move before newPos
  ++newPosIt;
  newPosInst = &*newPosIt;
  inst = (llvm::Instruction *)extraVals[0];
  inst->moveBefore(newPosInst);
  mutator->iitInTmp = inst->getIterator();
}

void RandomMoveHelper::debug() {
  llvm::errs() << "Instruction was moved around\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
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
  assert(llvm::isa<llvm::CallInst>(&*mutator->iitInTmp) &&
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
      rangePos.push_back(index);
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
  llvm::Function *func = mutator->functionInTmp;
  setFuncAttr(llvm::Attribute::AttrKind::NoFree, Random::getRandomBool());
  if (func->getReturnType()->isIntegerTy()) {
    llvm::IntegerType *intTy = (llvm::IntegerType *)func->getReturnType();
    size_t bitWidth = intTy->getBitWidth();
    if (Random::getRandomBool()) {
      llvm::Attribute constRange = llvm::Attribute::get(
          func->getContext(), llvm::Attribute::AttrKind::Range,
          Random::getRandomLLVMConstantRange(intTy));
      if (func->hasRetAttribute(llvm::Attribute::AttrKind::Range)) {
        func->removeRetAttr(llvm::Attribute::AttrKind::Range);
      }
      func->addRetAttr(constRange);
    }

    if (disableEXT.find(bitWidth) == disableEXT.end()) {
      setFuncRetAttr(llvm::Attribute::AttrKind::ZExt, false);
      setFuncRetAttr(llvm::Attribute::AttrKind::SExt, false);
      if (disableZEXT.find(bitWidth) != disableZEXT.end()) {
        setFuncRetAttr(llvm::Attribute::AttrKind::SExt,
                       Random::getRandomBool());
      } else if (disableSEXT.find(bitWidth) != disableSEXT.end()) {
        setFuncRetAttr(llvm::Attribute::AttrKind::ZExt,
                       Random::getRandomBool());
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

  for (size_t index : rangePos) {
    llvm::IntegerType *intTy =
        (llvm::IntegerType *)func->getArg(index)->getType();
    if (Random::getRandomBool()) {
      llvm::Attribute constRange = llvm::Attribute::get(
          func->getContext(), llvm::Attribute::AttrKind::Range,
          Random::getRandomLLVMConstantRange(intTy));
      if (func->hasRetAttribute(llvm::Attribute::AttrKind::Range)) {
        func->removeRetAttr(llvm::Attribute::AttrKind::Range);
      }
      func->addRetAttr(constRange);
    }
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

#define Pred llvm::CmpInst::Predicate
const std::vector<llvm::CmpInst::Predicate>
    BinaryInstructionHelper::ICmpPredicates({Pred::ICMP_EQ, Pred::ICMP_NE,
                                             Pred::ICMP_SGE, Pred::ICMP_SGT,
                                             Pred::ICMP_SLE, Pred::ICMP_SLT,
                                             Pred::ICMP_UGE, Pred::ICMP_UGT,
                                             Pred::ICMP_ULE, Pred::ICMP_ULT});
const std::vector<llvm::CmpInst::Predicate>
    BinaryInstructionHelper::FCmpPredicates(
        {Pred::FCMP_OEQ, Pred::FCMP_ONE, Pred::FCMP_OGE, Pred::FCMP_OGE,
         Pred::FCMP_OLT, Pred::FCMP_OLE, Pred::FCMP_ORD, Pred::FCMP_UNO,
         Pred::FCMP_UEQ, Pred::FCMP_UNE, Pred::FCMP_UGE, Pred::FCMP_UGT,
         Pred::FCMP_ULT, Pred::FCMP_ULE});
#undef Pred

void BinaryInstructionHelper::mutate() {
  if (llvm::isa<llvm::BinaryOperator>(&*mutator->iitInTmp)) {
    llvm::BinaryOperator *binInst =
        (llvm::BinaryOperator *)(&*mutator->iitInTmp);
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
  } else if (llvm::isa<llvm::CmpInst>(&*mutator->iitInTmp)) {
    llvm::CmpInst *binInst = (llvm::CmpInst *)(&*mutator->iitInTmp);
    if (Random::getRandomBool()) {
      binInst->swapOperands();
    }
    bool isFloat = binInst->getOperand(0)->getType()->isFPOrFPVectorTy();
    const std::vector<llvm::CmpInst::Predicate> &preds =
        isFloat ? FCmpPredicates : ICmpPredicates;
    size_t newP = Random::getRandomUnsigned() % preds.size();
    if (preds[newP] == binInst->getPredicate()) {
      newP += 1;
      newP %= preds.size();
    }
    binInst->setPredicate(preds[newP]);
  } else {
    assert(false && "not supported binary instruction");
  }
  updated = true;
}

bool BinaryInstructionHelper::canMutate(llvm::Function *func) {
  for (auto it = inst_begin(func); it != inst_end(func); ++it) {
    if (llvm::isa<llvm::BinaryOperator>(&*it) ||
        llvm::isa<llvm::CmpInst>(&*it)) {
      return true;
    }
  }
  return false;
}

bool BinaryInstructionHelper::shouldMutate() {
  return !updated && (llvm::isa<llvm::BinaryOperator>(&*mutator->iitInTmp) ||
                      llvm::isa<llvm::CmpInst>(&*mutator->iitInTmp));
}

void BinaryInstructionHelper::debug() {
  llvm::errs() << "\nBinaryInstructionHelper: Current binary inst:\n";
  mutator->iitInTmp->print(llvm::errs());
  llvm::errs() << "\n";
  mutator->iitInTmp->getParent()->print(llvm::errs());
}

bool ResizeIntegerHelper::isValidNode(llvm::Value *val) {
  if (llvm::isa<llvm::BinaryOperator>(val)) {
    llvm::Type *ty = val->getType();
    return ty->isIntOrIntVectorTy();
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
llvm::Type *ResizeIntegerHelper::getNewIntegerTy(llvm::LLVMContext &context,
                                                 llvm::Type *intTy) {
  static llvm::SmallVector<size_t> defaultWidth{1, 8, 16, 32, 64};
  assert(intTy->isIntOrIntVectorTy() && "ty should be an int or int vector ty");
  llvm::Type *result = nullptr;
  if (Random::getRandomBool()) {
    result = llvm::IntegerType::get(
        context,
        defaultWidth[Random::getRandomUnsigned() % defaultWidth.size()]);
  } else {
    result =
        llvm::IntegerType::get(context, 1 + Random::getRandomUnsigned() % 64);
  }

  if (intTy->isVectorTy()) {
    llvm::VectorType *vecTy = (llvm::VectorType *)intTy;
    result = llvm::VectorType::get(result, vecTy->getElementCount());
  }
  return result;
}

llvm::Type *ResizeIntegerHelper::getNewFPTy(llvm::LLVMContext &context,
                                            llvm::Type *FPTy) {
  assert(FPTy->isHalfTy() || FPTy->isBFloatTy() || FPTy->isFloatTy() ||
         FPTy->isDoubleTy());
  static llvm::SmallVector<llvm::Type *> FPTypes;
  if (FPTypes.size() != 4) {
    FPTypes = {llvm::Type::getHalfTy(context), llvm::Type::getBFloatTy(context),
               llvm::Type::getFloatTy(context),
               llvm::Type::getDoubleTy(context)};
  }

  auto it = FPTypes.begin();
  for (size_t i = Random::getRandomUnsigned() % FPTypes.size(); i != 0;
       --i, ++it)
    ;
  for (size_t i = 0; i < FPTypes.size(); ++i, ++it) {
    if (it == FPTypes.end()) {
      it = FPTypes.begin();
    }
    if (*it != FPTy) {
      return *it;
    }
  }
  return nullptr;
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
    assert(newOp->getType()->isIntOrIntVectorTy());
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
                                      llvm::Type *newIntTy) {
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
                                        llvm::Type *newTy) {
  assert(inst->getNumOperands() > index);
  llvm::Value *val = inst->getOperand(index);
  assert(llvm::isa<llvm::IntegerType>(val->getType()));

  llvm::Value *newOp = mutator_util::updateIntegerSize(val, newTy, inst);
  inst->setOperand(index, newOp);
}

void ResizeIntegerHelper::mutate() {
  llvm::Instruction *inst = &*mutator->iitInTmp;
  std::vector<llvm::Instruction *> useChain = constructUseChain(inst);
  llvm::Type *oldIntTy = inst->getType(), *newIntTy = oldIntTy;
  do {
    newIntTy = getNewIntegerTy(inst->getContext(), oldIntTy);
  } while (newIntTy == oldIntTy);
  updateChain(useChain, newIntTy);
  updated = true;
}

void ResizeIntegerHelper::debug() {
  llvm::errs() << "integer resized\n";
  mutator->iitInTmp->getParent()->print(llvm::errs());
  llvm::errs() << "\n";
}