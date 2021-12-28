#include "ComplexMutator.h"

void ShuffleHelper::init(){
    for(auto fit=mutator->pm->begin();fit!=mutator->pm->end();++fit){
        if(fit->isDeclaration()||mutator->invalidFunctions.find(fit->getName().str())!=mutator->invalidFunctions.end()){
            continue;
        }
        shuffleBasicBlockIndex=0;
        /**
         * find the same location as iit,bit,fit, and set shuffleBasicBlock
         * 
         */
        for(auto bit=fit->begin();bit!=fit->end();++bit,++shuffleBasicBlockIndex){
            for(auto iit=bit->begin();iit!=bit->end();++iit){
                if(&*iit==(&*(mutator->iit))){
                    goto varSetEnd;
                }
            }
        }
    }
varSetEnd:

    for(auto funcIt=mutator->pm->begin();funcIt!=mutator->pm->end();++funcIt){
        if(!funcIt->isDeclaration()&&!funcIt->getName().empty()
                &&mutator->invalidFunctions.find(funcIt->getName().str())==mutator->invalidFunctions.end()){
            /*
                Handle shuffle map
            */
            shuffleMap.insert(std::make_pair(funcIt->getName(),FunctionShuffleBlock()));
            FunctionShuffleBlock& fSBlock=shuffleMap[funcIt->getName()];
            fSBlock.resize(funcIt->size());
            size_t idx=0;
            for(auto bbIt=funcIt->begin();bbIt!=funcIt->end();++bbIt,++idx){
                BasicBlockShuffleBlock& bSBlock=fSBlock[idx];
                ShuffleBlock tmp;
                std::unordered_set<llvm::Value*> us;
                auto instIt=bbIt->begin();
                /**
                 * handle phi instructions at beginning.
                 */
                while(!instIt->isTerminator()&&llvm::isa<llvm::PHINode>(&*instIt)){
                    tmp.push_back(&*instIt);
                    ++instIt;
                }
                if(tmp.size()>=2){
                    bSBlock.push_back(tmp);
                }
                tmp.clear();
                for(;!instIt->isTerminator();++instIt){
                    bool flag=true;
                    for(size_t op=0;flag&&op<instIt->getNumOperands();++op){
                        if(us.find(instIt->getOperand(op))!=us.end()){
                            flag=false;
                        }
                    }
                    if(!flag){
                        if(tmp.size()>=2){
                            bSBlock.push_back(tmp);
                            
                        }
                        tmp.clear();
                        us.clear();
                    }
                    tmp.push_back(&*instIt);
                    us.insert(&*instIt);
                }
                if(tmp.size()>=2){
                    bSBlock.push_back(tmp);
                }
            }
        }
    }
}

bool ShuffleHelper::shouldMutate(){
    return shuffleMap[mutator->fit->getName()][shuffleBasicBlockIndex].size()>shuffleBlockIndex;
}

void ShuffleHelper::shuffleBlock(){
    ShuffleBlock& sblock=shuffleMap[mutator->fit->getName()][shuffleBasicBlockIndex][shuffleBlockIndex];
    llvm::SmallVector<llvm::Instruction*> sv;
    for(const auto& p:sblock){
        sv.push_back(p);
    }
    llvm::Instruction* nextInst=(llvm::Instruction*)&*(mutator->vMap)[&*(++sv.back()->getIterator())];
    while(sv==sblock){
        std::random_shuffle(sv.begin(),sv.end());
    }
    for(llvm::Instruction* p:sv){
        ((llvm::Instruction*)&*(mutator->vMap)[p])->removeFromParent();
    }
    
    for(llvm::Instruction* p:sv){
        ((llvm::Instruction*)&*(mutator->vMap)[p])->insertBefore(nextInst);
    }
    mutator->tmpIit=llvm::BasicBlock::iterator((llvm::Instruction*)&*mutator->vMap[&*mutator->iit]);
}

void MutateInstructionHelper::mutate(){
    //75% chances to add a new inst, 25% chances to replace with a existent usage
    if((Random::getRandomUnsigned()&3)!=0){
        insertRandomBinaryInstruction(&*(mutator->tmpIit));
        newAdded=true;
    }else{
        replaceRandomUsage(&*(mutator->tmpIit));
    }
    mutated=true;
};

void MutateInstructionHelper::insertRandomBinaryInstruction(llvm::Instruction* inst){
    size_t pos=Random::getRandomUnsigned()%inst->getNumOperands();
    llvm::Type* ty=nullptr;
    for(size_t i=0;i<inst->getNumOperands();++i,++pos){
        if(pos==inst->getNumOperands())pos=0;
        if(inst->getOperand(pos)->getType()->isIntegerTy()){
            ty=inst->getOperand(pos)->getType();
            break;
        }
    }
    
    llvm::Value* val1=mutator->getRandomValue(ty),*val2=mutator->getRandomValue(ty);
    llvm::Instruction::BinaryOps Op;

    using llvm::Instruction;
    switch (Random::getRandomUnsigned() % 13) {
        default: llvm_unreachable("Invalid BinOp");
        case 0:{Op = Instruction::Add; break; }
        case 1:{Op = Instruction::Sub; break; }
        case 2:{Op = Instruction::Mul; break; }
        case 3:{Op = Instruction::SDiv; break; }
        case 4:{Op = Instruction::UDiv; break; }
        case 5:{Op = Instruction::SRem; break; }
        case 6:{Op = Instruction::URem; break; }
        case 7: {Op = Instruction::Shl;  break; }
        case 8: {Op = Instruction::LShr; break; }
        case 9: {Op = Instruction::AShr; break; }
        case 10:{Op = Instruction::And;  break; }
        case 11:{Op = Instruction::Or;   break; }
        case 12:{Op = Instruction::Xor;  break; }
    }

    llvm::Instruction* newInst=llvm::BinaryOperator::Create(Op, val1, val2, "", inst);
    inst->setOperand(pos,newInst);
}

void MutateInstructionHelper::replaceRandomUsage(llvm::Instruction* inst){
    size_t pos=Random::getRandomUnsigned()%inst->getNumOperands();;
    llvm::Type* ty=nullptr;
    for(size_t i=0;i<inst->getNumOperands();++i,++pos){
        if(pos==inst->getNumOperands())pos=0;
        if(inst->getOperand(pos)->getType()->isIntegerTy()){
            ty=inst->getOperand(pos)->getType();
            break;
        }
    }
    llvm::Value* val=mutator->getRandomValue(ty);
    inst->setOperand(pos,val);
}

bool RandomMoveHelper::shouldMutate(){
    return !moved&&mutator->tmpBit->size()>2&&!mutator->tmpIit->isTerminator();
}

void RandomMoveHelper::mutate(){
    randomMoveInstruction(&*(mutator->tmpIit));
    mutator->extraFuncArgs.clear();
}

void RandomMoveHelper::randomMoveInstruction(llvm::Instruction* inst){
    if(inst->getNextNonDebugInstruction()->isTerminator()){
    //if(Random::getRandomBool()){
        randomMoveInstructionForward(inst);
    }else if(inst==&*(inst->getParent()->begin())){
        randomMoveInstructionBackward(inst);
    }else{
        if(Random::getRandomBool()){
            randomMoveInstructionForward(inst);
        }else{
            randomMoveInstructionBackward(inst);
        }
    }
}

void RandomMoveHelper::randomMoveInstructionForward(llvm::Instruction* inst){
    size_t pos=0,newPos,beginPos=0;

    for(auto it=inst->getParent()->begin();&*it!=inst;++it,++pos);
    /**
     * PHINode must be the first inst in the basic block.
     * 
     */
    if(!llvm::isa<llvm::PHINode>(inst)){
        for(llvm::Instruction* phiInst=&*inst->getParent()->begin();llvm::isa<llvm::PHINode>(phiInst);phiInst=phiInst->getNextNonDebugInstruction()){
            ++beginPos;
        }
    }

    /*
     *  Current inst cannot move forward because current inst is not PHI inst,
     *  and there are zero or more PHI inst(s) in front of current inst.
     */
    if(pos==beginPos){
        return;
    }

    newPos=Random::getRandomUnsigned()%(pos-beginPos)+beginPos;
    //llvm::errs()<<pos<<' '<<beginPos<<' '<<newPos<<"AAAAAAAAAAAAAA\n";
    //llvm::errs()<<"both pos: "<<pos<<' '<<newPos<<"\n";
    llvm::SmallVector<llvm::Instruction*> v;
    llvm::SmallVector<llvm::Value*> domBackup;
    llvm::Instruction* newPosInst=inst;
    llvm::BasicBlock::iterator newPosIt=newPosInst->getIterator();
    for(size_t i=pos;i!=newPos;--i){
        --newPosIt;
        v.push_back(&*newPosIt);
        //remove Insts in current basic block
        domBackup.push_back(mutator->domInst.back());
        mutator->domInst.pop_back();
    }
    newPosInst=&*newPosIt;

    for(size_t i=0;i<inst->getNumOperands();++i){
        if(llvm::Value* op=inst->getOperand(i);std::find(v.begin(),v.end(),op)!=v.end()){
            mutator->setOperandRandomValue(inst,i);
        }
    }

    inst->moveBefore(newPosInst);

    mutator->fixAllValues();

    //restore domInst
    while(!domBackup.empty()){
        mutator->domInst.push_back(domBackup.back());
        domBackup.pop_back();
    }
}

void RandomMoveHelper::randomMoveInstructionBackward(llvm::Instruction* inst){
    size_t pos=0,newPos,endPos=0;

    for(auto it=inst->getParent()->begin();&*it!=inst;++it,++pos);
    if(llvm::isa<llvm::PHINode>(inst)){
        for(llvm::Instruction* phiInst=&*inst->getParent()->begin();llvm::isa<llvm::PHINode>(phiInst);phiInst=phiInst->getNextNonDebugInstruction()){
            ++endPos;
        }
    }
    if(endPos==0){
        endPos=inst->getParent()->size()-1;
    }
    /*
     * Current inst is a phi instruction and is the end of phi instruction block.
     */
    if(pos+1==endPos){
        return;
    }

    newPos=Random::getRandomInt()%(endPos-pos)+1+pos;    

    //need fix all insts used current inst in [pos,newPos]
    llvm::Instruction* newPosInst=inst;
    for(size_t i=pos;i!=newPos;++i){
        newPosInst=inst->getNextNonDebugInstruction();
        for(size_t op=0;op<newPosInst->getNumOperands();++op){
            if(llvm::Value* opP=newPosInst->getOperand(op);opP!=nullptr&&opP==inst){
                mutator->setOperandRandomValue(newPosInst,op);
            }
        }
    }

    inst->moveBefore(newPosInst);
    mutator->fixAllValues();
}