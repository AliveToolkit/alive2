#include "ComplexMutator.h"

void ShuffleHelper::init() {
  for (auto fit = mutator->pm->begin(); fit != mutator->pm->end(); ++fit) {
    if (fit->isDeclaration() ||
        mutator->invalidFunctions.find(fit->getName().str()) !=
            mutator->invalidFunctions.end()) {
      continue;
    }
    shuffleBasicBlockIndex = 0;
    /**
     * find the same location as iit,bit,fit, and set shuffleBasicBlock
     *
     */
    for (auto bit = fit->begin(); bit != fit->end();
         ++bit, ++shuffleBasicBlockIndex) {
      for (auto iit = bit->begin(); iit != bit->end(); ++iit) {
        if (&*iit == (&*(mutator->iit))) {
          goto varSetEnd;
        }
      }
    }
  }
varSetEnd:

  for (auto funcIt = mutator->pm->begin(); funcIt != mutator->pm->end();
       ++funcIt) {
    if (!funcIt->isDeclaration() && !funcIt->getName().empty() &&
        mutator->invalidFunctions.find(funcIt->getName().str()) ==
            mutator->invalidFunctions.end()) {
      /*
          Handle shuffle map
      */
      shuffleMap.insert(
          std::make_pair(funcIt->getName(), FunctionShuffleBlock()));
      FunctionShuffleBlock &fSBlock = shuffleMap[funcIt->getName()];
      fSBlock.resize(funcIt->size());
      size_t idx = 0;
      for (auto bbIt = funcIt->begin(); bbIt != funcIt->end(); ++bbIt, ++idx) {
        BasicBlockShuffleBlock &bSBlock = fSBlock[idx];
        ShuffleBlock tmp;
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
  }
}

bool ShuffleHelper::shouldMutate() {
  return shuffleMap[mutator->fit->getName()][shuffleBasicBlockIndex].size() >
         shuffleBlockIndex;
}

void ShuffleHelper::shuffleBlock() {
  ShuffleBlock &sblock = shuffleMap[mutator->fit->getName()]
                                   [shuffleBasicBlockIndex][shuffleBlockIndex];
  llvm::SmallVector<llvm::Instruction *> sv;
  for (const auto &p : sblock) {
    sv.push_back(p);
  }
  int idx = mutator->domInst.find(sv[0]);
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
      for (size_t i = 0; i + idx < mutator->domInst.size() && i < sv.size();
           ++i) {
        mutator->domInst[i + idx] = sv[i];
      }
    }
  } else {
    while (findInSV--) {
      mutator->domInst.pop_back_tmp();
    }
    for (size_t i = 0; i < sv.size() && sv[i]->getIterator() != mutator->iit;
         ++i) {
      mutator->domInst.push_back_tmp(sv[i]);
    }
  }

  for (llvm::Instruction *p : sv) {
    ((llvm::Instruction *)&*(mutator->vMap)[p])->removeFromParent();
  }

  for (llvm::Instruction *p : sv) {
    ((llvm::Instruction *)&*(mutator->vMap)[p])->insertBefore(nextInst);
  }
  mutator->tmpIit = llvm::BasicBlock::iterator(
      (llvm::Instruction *)&*mutator->vMap[&*mutator->iit]);
}

bool MutateInstructionHelper::shouldMutate() {
  return !mutated && 
    (mutator->tmpIit->getNumOperands()-llvm::isa<CallBase>(&*(mutator->tmpIit)))>0;
}

void MutateInstructionHelper::mutate() {
  //do extra handling for br insts
  if(llvm::isa<llvm::BranchInst>(mutator->tmpIit)){
    llvm::BranchInst* brInst=(llvm::BranchInst*)&*mutator->tmpIit;
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
    }
  }
  // 75% chances to add a new inst, 25% chances to replace with a existent usage
  else if ((Random::getRandomUnsigned() & 3) != 0) {
    bool res=insertRandomBinaryInstruction(&*(mutator->tmpIit));
    if(!res){
      replaceRandomUsage(&*(mutator->tmpIit));
    }
    newAdded=res;
  } else {
    replaceRandomUsage(&*(mutator->tmpIit));
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

  llvm::Value *val1 = mutator->getRandomValue(ty),
              *val2 = mutator->getRandomValue(ty);
  llvm::Instruction::BinaryOps Op;

  using llvm::Instruction;
  switch (Random::getRandomUnsigned() % 13) {
  default:
    llvm_unreachable("Invalid BinOp");
  case 0: {
    Op = Instruction::Add;
    break;
  }
  case 1: {
    Op = Instruction::Sub;
    break;
  }
  case 2: {
    Op = Instruction::Mul;
    break;
  }
  case 3: {
    Op = Instruction::SDiv;
    break;
  }
  case 4: {
    Op = Instruction::UDiv;
    break;
  }
  case 5: {
    Op = Instruction::SRem;
    break;
  }
  case 6: {
    Op = Instruction::URem;
    break;
  }
  case 7: {
    Op = Instruction::Shl;
    break;
  }
  case 8: {
    Op = Instruction::LShr;
    break;
  }
  case 9: {
    Op = Instruction::AShr;
    break;
  }
  case 10: {
    Op = Instruction::And;
    break;
  }
  case 11: {
    Op = Instruction::Or;
    break;
  }
  case 12: {
    Op = Instruction::Xor;
    break;
  }
  }

  llvm::Instruction *newInst =
      llvm::BinaryOperator::Create(Op, val1, val2, "", inst);
  inst->setOperand(pos, newInst);
  return true;
}

void MutateInstructionHelper::replaceRandomUsage(llvm::Instruction *inst) {
  size_t pos = Random::getRandomUnsigned() % inst->getNumOperands();
  
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
  mutator->fixAllValues();

}

bool RandomMoveHelper::shouldMutate() {
  return !moved && mutator->tmpBit->size() > 2 &&
         !mutator->tmpIit->isTerminator();
}

void RandomMoveHelper::mutate() {
  randomMoveInstruction(&*(mutator->tmpIit));
  moved = true;
  mutator->extraValue.clear();
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
    assert(mutator->domInst.tmp_size() != 0);
    mutator->domInst.pop_back_tmp();
  }
  newPosInst = &*newPosIt;

  for (size_t i = 0; i < inst->getNumOperands(); ++i) {
    if (llvm::Value *op = inst->getOperand(i);
        std::find(v.begin(), v.end(), op) != v.end()) {
      mutator->setOperandRandomValue(inst, i);
    }
  }
  inst->moveBefore(newPosInst);

  mutator->fixAllValues();
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
  for (size_t i = pos; i != newPos; ++i) {
    ++newPosIt;
    newPosInst = &*newPosIt;
    for (size_t op = 0; op < newPosInst->getNumOperands(); ++op) {
      if (llvm::Value *opP = newPosInst->getOperand(op);
          opP != nullptr && opP == inst) {
        mutator->setOperandRandomValue(newPosInst, op);
      }
    }
    mutator->fixAllValues();
    mutator->extraValue.push_back(newPosInst);
  }

  inst->moveBefore(newPosInst);
}

bool RandomCodeInserterHelper::shouldMutate() {
  return !generated && !llvm::isa<llvm::PHINode>(mutator->tmpIit);
}

void RandomCodeInserterHelper::mutate() {
  generated = true;
  // if not the first inst of this block, we can do a split
  llvm::Instruction *insertPoint = &*mutator->tmpIit;
  if (mutator->tmpBit->getFirstNonPHIOrDbg() != insertPoint) {
    llvm::BasicBlock* oldBB=&*mutator->tmpBit;
    llvm::BasicBlock* newBB=mutator->tmpBit->splitBasicBlock(mutator->tmpIit);
    oldBB->replaceSuccessorsPhiUsesWith(newBB);
  }
  LLVMUtil::insertRandomCodeBefore(insertPoint);
}

void FunctionCallInlineHelper::init(){
  if(funcToId.empty()){
    for(auto fit=mutator->pm->begin(); fit != mutator->pm->end();++fit){
      if(fit->isDeclaration()){
        continue;
      }
      bool shouldAdd=true;
      for(size_t i=0;i<idToFuncSet.size()&&shouldAdd;++i){
        if(LLVMUtil::compareSignature(&*fit,mutator->pm->getFunction(idToFuncSet[i][0]))){
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
  if(!inlined&& llvm::isa<llvm::CallInst>(mutator->tmpIit)){
     llvm::CallInst* callInst=(llvm::CallInst*)&*mutator->tmpIit;
     llvm::Function* func=callInst->getCalledFunction();
     if(!func->isDeclaration()){
       auto it=funcToId.find(func->getName());
       if(it!=funcToId.end()&&idToFuncSet[it->second].size()>1){
         //make sure there is a replacement
          size_t idx=Random::getRandomUnsigned()%idToFuncSet[it->second].size();
          while(idToFuncSet[it->second][idx]==func->getName()){
            ++idx;
            if(idx==idToFuncSet[it->second].size()){
              idx=0;
            }
          }
          functionInlined=idToFuncSet[it->second][idx];
       }
     }
  }
  return !inlined&&!functionInlined.empty();
}

void FunctionCallInlineHelper::mutate() {
  inlined = true;
  llvm::InlineFunctionInfo ifi;
  llvm::Function* func=mutator->tmpCopy->getFunction(functionInlined);
  llvm::CallInst* callInst=(llvm::CallInst*)(&*mutator->tmpIit);
  callInst->setCalledFunction(func);
  llvm::InlineResult res=llvm::InlineFunction(*callInst,ifi);
  if(!res.isSuccess()){
    llvm::errs()<<res.getFailureReason()<<"\n";
  }
}
