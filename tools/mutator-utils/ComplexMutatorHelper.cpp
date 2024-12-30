#include "ComplexMutator.h"

void ShuffleHelper::init() {
    shuffleUnitInBasicBlockIndex = 0;
    /**
     * find the same location as iit,bit,fit, and set shuffleBasicBlock
     *
     */
    for (auto bit = mutator->currentFunction->begin(); bit != mutator->currentFunction->end();
         ++bit, ++shuffleUnitInBasicBlockIndex) {
      for (auto iit = bit->begin(); iit != bit->end(); ++iit) {
        if (&*iit == (&*(mutator->iit))) {
          goto varSetEnd;
        }
      }
    }
varSetEnd:
  llvm::Function* func=mutator->currentFunction;
    shuffleBlockInFunction.resize(func->size());
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
  ShuffleUnit &sblock = shuffleBlockInFunction[shuffleUnitInBasicBlockIndex]
                                   [shuffleUnitIndex];
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
    std::random_shuffle(sv.begin(), sv.end(),[](int i){return Random::getRandomUnsigned()%i;});
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

bool MutateInstructionHelper::shouldMutate() {
  bool allBasicBlockOrFunc=true;
  //make sure at least one 
  for(size_t i=0;allBasicBlockOrFunc&&i<mutator->iitInTmp->getNumOperands();i++){
    if(!llvm::isa<llvm::BasicBlock>(mutator->iitInTmp->getOperand(i))&&!llvm::isa<llvm::Function>(mutator->iitInTmp->getOperand(i))){
      allBasicBlockOrFunc=false;
    }
  }
  return !mutated && !allBasicBlockOrFunc &&
    (mutator->iitInTmp->getNumOperands()-llvm::isa<CallBase>(&*(mutator->iitInTmp)))>0
    //cannot be a LangdingPadInst, its catch clause requires the value has to be a global variable.
    && !llvm::isa<llvm::LandingPadInst>(mutator->iitInTmp)
    //The ret value of CleanupRet Inst must be a CleanupPad, needs extra check so ignore for now.
    && !llvm::isa<llvm::CleanupReturnInst>(mutator->iitInTmp)
    //all catch related inst require the value has to be label
    && !llvm::isa<llvm::CatchPadInst>(mutator->iitInTmp)
    && !llvm::isa<llvm::CatchSwitchInst>(mutator->iitInTmp)
    && !llvm::isa<llvm::CatchReturnInst>(mutator->iitInTmp);
}

void MutateInstructionHelper::mutate() {
  //do extra handling for br insts
  if(llvm::isa<llvm::BranchInst>(mutator->iitInTmp)){
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
    bool res=insertRandomBinaryInstruction(&*(mutator->iitInTmp));
    if(!res){
      replaceRandomUsage(&*(mutator->iitInTmp));
    }
    newAdded=res;
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

  if(ty==nullptr){
    return false;
  }

  if(!ty->isFloatingPointTy()&&!ty->isIntegerTy()){
    return false;
  }

  bool isFloat=ty->isFloatingPointTy();

  llvm::Value *val1 = mutator->getRandomValue(ty),
              *val2 = mutator->getRandomValue(ty);
  llvm::Instruction* newInst=nullptr;
  if(isFloat){
    newInst=LLVMUtil::getRandomFloatInstruction(val1,val2,inst);
  }else{
    newInst=LLVMUtil::getRandomIntegerInstruction(val1,val2,inst);
  }
  inst->setOperand(pos, newInst);
  return true;
}

void MutateInstructionHelper::replaceRandomUsage(llvm::Instruction *inst) {
  bool found=false;
  size_t pos=Random::getRandomUnsigned() % inst->getNumOperands();
  //make sure at least one 
  for(size_t i=0;!found&&i<mutator->iitInTmp->getNumOperands();i++,pos++){
    if(pos==mutator->iitInTmp->getNumOperands()){
      pos=0;
    }
    if(!llvm::isa<llvm::BasicBlock>(mutator->iitInTmp->getOperand(pos))&&!llvm::isa<llvm::Function>(mutator->iitInTmp->getOperand(pos))){
      found=true;
      break;
    }
  }
  assert(found && "at least should find a location which is not a basic block and function!");
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
  mutator->setOperandRandomValue(inst,pos);
  llvm::SmallVector<llvm::Value*> vals;
  mutator->fixAllValues(vals);

}

bool RandomMoveHelper::shouldMutate() {
  return !moved && mutator->bitInTmp->size() > 2 &&
         !mutator->iitInTmp->isTerminator();
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
  for (auto it = inst->getParent()->begin(); &*it != inst; ++it, ++pos)
    ;
  /**
   * PHINode must be the first inst in the basic block.
   *
   */
  if (!llvm::isa<llvm::PHINode>(inst)) {
    for (llvm::Instruction *phiInst = &*inst->getParent()->begin();
         llvm::isa<llvm::PHINode>(phiInst);
         phiInst = phiInst->getNextNonDebugInstruction()) {
      ++beginPos;
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

  for (size_t i = 0; i < inst->getNumOperands(); ++i) {
    if (llvm::Value *op = inst->getOperand(i);
        std::find(v.begin(), v.end(), op) != v.end()) {
      mutator->setOperandRandomValue(inst, i);
    }
  }
  inst->moveBefore(newPosInst);
  llvm::SmallVector<llvm::Value*> vals;
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
  llvm::SmallVector<llvm::Value*> extraVals;
  extraVals.push_back(inst);
  extraVals.push_back(newPosInst);
  for (size_t i = pos; i != newPos; ++i) {
    ++newPosIt;
    newPosInst = &*newPosIt;
    for (size_t op = 0; op < newPosInst->getNumOperands(); ++op) {
      if (llvm::Value *opP = newPosInst->getOperand(op);
          opP != nullptr && opP == inst) {
        mutator->setOperandRandomValue(newPosInst, op);
      }
    }
    //llvm::errs()<<"\nAAAAAAAAAAAAAA\n";
    //mutator->domInst.back()->print(llvm::errs());
    mutator->fixAllValues(extraVals);
    newPosInst=(llvm::Instruction*)extraVals[1];
    newPosIt=newPosInst->getIterator();
    mutator->extraValues.push_back(newPosInst);
  }
  inst=(llvm::Instruction*)extraVals[0];
  inst->moveBefore(newPosInst);
}

bool RandomCodeInserterHelper::shouldMutate() {
  return !generated && !llvm::isa<llvm::PHINode>(mutator->iitInTmp);
}

void RandomCodeInserterHelper::mutate() {
  generated = true;
  // if not the first inst of this block, we can do a split
  llvm::Instruction *insertPoint = &*mutator->iitInTmp;
  if (mutator->bitInTmp->getFirstNonPHIOrDbg() != insertPoint) {
    llvm::BasicBlock* oldBB=&*mutator->bitInTmp;
    llvm::Instruction* inst=oldBB->getTerminator();
    llvm::SmallVector<llvm::BasicBlock*> succs;
    for(size_t i=0;i<inst->getNumOperands();++i){
      if(llvm::Value* val=inst->getOperand(i);llvm::isa<BasicBlock>(val)){
        succs.push_back((llvm::BasicBlock*)val);
      }
    }
    llvm::BasicBlock* newBB=mutator->bitInTmp->splitBasicBlock(mutator->iitInTmp);
    for(auto bb:succs){
      bb->replacePhiUsesWith(oldBB,newBB);
    }
  }
  LLVMUtil::insertRandomCodeBefore(insertPoint);
}

void FunctionCallInlineHelper::init(){
  if(funcToId.empty()){
    llvm::Module* module=mutator->currentFunction->getParent();
    for(auto fit=module->begin(); fit != module->end();++fit){
      if(fit->isDeclaration()){
        continue;
      }
      bool shouldAdd=true;
      for(size_t i=0;i<idToFuncSet.size()&&shouldAdd;++i){
        if(LLVMUtil::compareSignature(&*fit,module->getFunction(idToFuncSet[i][0]))){
          funcToId.insert(std::make_pair(fit->getName(),funcToId[idToFuncSet[i][0]]));
          idToFuncSet[i].push_back(fit->getName().str());
          shouldAdd=false;
        }
      }
      if(shouldAdd){
        funcToId.insert(std::make_pair(fit->getName(),idToFuncSet.size()));
        idToFuncSet.push_back(std::vector<std::string>());
        idToFuncSet.back().push_back(fit->getName().str());
      }
    }
  }
  inlined=false;
}

/*
* Not inlined. 
* is a function call
* could find replacement
*/
bool FunctionCallInlineHelper::shouldMutate() {
  if(!inlined&& llvm::isa<llvm::CallInst>(mutator->iitInTmp)){
     llvm::CallInst* callInst=(llvm::CallInst*)&*mutator->iitInTmp;
     llvm::Function* func=callInst->getCalledFunction();
     return func!=nullptr&&!func->isDeclaration();
  }
  return false;
}

llvm::Function* FunctionCallInlineHelper::getReplacedFunction(){
  assert(llvm::isa<llvm::CallInst>(mutator->iitInTmp)&&"function inline should be a call inst");
  llvm::CallInst* callInst=(llvm::CallInst*)&*mutator->iitInTmp;
  llvm::Function* func=callInst->getCalledFunction();
  functionInlined=func->getName();
  auto it=funcToId.find(func->getName());
  if(it!=funcToId.end()&&!idToFuncSet[it->second].empty()){
    //make sure there is a replacement
    size_t idx=Random::getRandomUnsigned()%idToFuncSet[it->second].size();
    functionInlined=idToFuncSet[it->second][idx];
    //final check the inlined function must have the same type with called function
    //because of the pre-calculated function signature might be added with more args
    if(mutator->tmpCopy->getFunction(functionInlined)->getFunctionType()!=func->getFunctionType()){
      functionInlined=func->getName();
    }
  }
  return mutator->tmpCopy->getFunction(functionInlined);
}

void FunctionCallInlineHelper::mutate() {
  inlined = true;
  llvm::InlineFunctionInfo ifi;
  llvm::Function* func=getReplacedFunction();
  llvm::CallInst* callInst=(llvm::CallInst*)(&*mutator->iitInTmp);
  callInst->setCalledFunction(func);
  llvm::InlineResult res=llvm::InlineFunction(*callInst,ifi);
  if(!res.isSuccess()){
    llvm::errs()<<res.getFailureReason()<<"\n";
  }
}
