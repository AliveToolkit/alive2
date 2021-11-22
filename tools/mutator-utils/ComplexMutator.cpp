#include "ComplexMutator.h"

bool ComplexMutator::init(){
    bool result=false;
    for(fit=pm->begin();fit!=pm->end();++fit){
        if(fit->isDeclaration()){
            continue;
        }
        for(bit=fit->begin();bit!=fit->end();++bit){
            for(iit=bit->begin();iit!=bit->end();++iit){
                if(isReplaceable(&*iit)){
                    result=true;
                    moved=false;
                    goto end;
                }
            }
        }
    }

end:
    if(result){
        for(auto funcIt=pm->begin();funcIt!=pm->end();++funcIt){
            for(auto ait=funcIt->arg_begin();ait!=funcIt->arg_end();++ait){
                if(ait->hasAttribute(llvm::Attribute::AttrKind::ImmArg)){
                    filterSet.insert(funcIt->getName().str());
                    break;
                }
            }
            
            if(!funcIt->isDeclaration()&&!funcIt->getName().empty()){
            /*
                Handle Dominator tree
            */
                dtMap[funcIt->getName()]=llvm::DominatorTree(*funcIt);
            }
        }

        for(auto git=pm->global_begin();git!=pm->global_end();++git){
            domInst.push_back(&*git);
        }
        calcDomInst();
        /*
          Hard code for when to update helpers 
        */
        helpers.push_back(std::make_unique<ShuffleHelper>(this));
        whenMoveToNextFuncFuncs.push_back(helpers.size()-1);
        whenMoveToNextBasicBlockFuncs.push_back(helpers.size()-1);

        helpers.push_back(std::make_unique<MutateInstructionHelper>(this));
        whenMoveToNextInstFuncs.push_back(helpers.size()-1);

        helpers.push_back(std::make_unique<RandomMoveHelper>(this));
        whenMoveToNextInstFuncs.push_back(helpers.size()-1);
        for(size_t i=0;i<helpers.size();++i){
            helpers[i]->init();
        }
        currHelpersIt=helpers.begin();
    }
    return result;
}

void ComplexMutator::resetTmpModule(){
    vMap.clear();
    tmpCopy=llvm::CloneModule(*pm,vMap);
    tmpFit=llvm::Module::iterator((llvm::Function*)&*vMap[&*fit]);
    tmpBit=llvm::Function::iterator((llvm::BasicBlock*)&*vMap[&*bit]);
    tmpIit=llvm::BasicBlock::iterator((llvm::Instruction*)&*vMap[&*iit]);
}

void ComplexMutator::mutateModule(const std::string& outputFileName){
    resetTmpModule();    
    if(debug){
        llvm::errs()<<"Current function "<<tmpFit->getName()<<"\n";
        llvm::errs()<<"Current basic block:\n";
        tmpBit->print(llvm::errs());
        llvm::errs()<<"\nCurrent instruction:\n";
        tmpIit->print(llvm::errs());
        llvm::errs()<<"\n";

    }
    currFuncName=tmpFit->getName().str();
    if((*currHelpersIt)->shouldMutate()){
        (*currHelpersIt)->mutate();
    }
    /*if(false&&shuffleMap[fit->getName()][shuffleBasicBlockIndex].size()>shuffleBlockIndex){
        //llvm::errs()<<"current shuffle size"<<shuffleMap[fit->getName()][shuffleBasicBlockIndex].size()<<"\n";
        shuffleBlock();
        ++shuffleBlockIndex;
        shuffled=true;
    }else if(!moved&&tmpBit->size()>2&&!tmpIit->isTerminator()){
        randomMoveInstruction(&*tmpIit);
        moveAround=true;
    }
    //75% chances to add a new inst, 25% chances to replace with a existent usage
    else if(false&&(Random::getRandomUnsigned()&3)!=0){
        insertRandomBinaryInstruction(&*tmpIit);
        newAdded=true;
    }else if(false){
        replaceRandomUsage(&*tmpIit);
    }*/
    if(debug){
        /*if(shuffled){
            llvm::errs()<<"\nInstructions shuffled\n";
        }else if(moveAround){
            llvm::errs()<<"\nInst was moved around\n";
        }else if(!newAdded){
            llvm::errs()<<"\nReplaced with a existant usage\n";
        }else{
            llvm::errs()<<"\nNew Inst added\n";
        }*/
        (*currHelpersIt)->debug();
        tmpBit->print(llvm::errs());
        llvm::errs()<<"\nDT info"<<dtMap.find(fit->getName())->second.dominates(&*(fit->getFunction().begin()->begin()),&*iit);
        llvm::errs()<<"\n";
    }
    while(currHelpersIt!=helpers.end()&&!(*currHelpersIt)->shouldMutate()){
        ++currHelpersIt;
    }
    if(currHelpersIt==helpers.end()){
        moveToNextReplaceableInst();
    }
}

void ComplexMutator::saveModule(const std::string& outputFileName){
    std::error_code ec;
    llvm::raw_fd_ostream fout(outputFileName,ec);
    fout<<*tmpCopy;
    fout.close();
    llvm::errs()<<"file wrote to "<<outputFileName<<"\n";
}

bool ComplexMutator::isReplaceable(llvm::Instruction* inst){
    //contain immarg attributes
    if(llvm::isa<llvm::CallBase>(inst)){
        //in case of cannot find function name
        if(llvm::Function* func=((llvm::CallBase*)inst)->getCalledFunction();func!=nullptr&&
            (filterSet.find(func->getName().str())!=filterSet.end()||func->getName().startswith("llvm"))){
            return false;
        }
    }
    //don't do replacement on PHI node
    //don't update an alloca inst
    //don't do operations on Switch inst for now.
    if(llvm::isa<llvm::PHINode>(inst)||llvm::isa<llvm::GetElementPtrInst>(inst)||llvm::isa<llvm::AllocaInst>(inst)
        ||llvm::isa<llvm::SwitchInst>(inst)){
        return false;
    }

    //only consider inst within an integer type
    for(llvm::Use& u:inst->operands()){
        if(u.get()->getType()->isIntegerTy()){
            return true;
        }
    }
    return false;
}

void ComplexMutator::moveToNextFuction(){
    ++fit;
    if(fit==pm->end())fit=pm->begin();
    while(fit->isDeclaration()){
        ++fit;if(fit==pm->end())fit=pm->begin();
    }
    bit=fit->begin();
    iit=bit->begin();
    //shuffleBasicBlockIndex=0;
    for(size_t i:whenMoveToNextFuncFuncs){
        helpers[i]->whenMoveToNextFunction();
    }
}

void ComplexMutator::moveToNextBasicBlock(){
    ++bit;
    if(bit==fit->end()){
        moveToNextFuction();
    }else{
        iit=bit->begin();
    }
    calcDomInst();
    //shuffleBlockIndex=0;
    for(size_t i:whenMoveToNextBasicBlockFuncs){
        helpers[i]->whenMoveToNextBasicBlock();
    }
}

void ComplexMutator::moveToNextInst(){
    domInst.push_back(&*iit);
    ++iit;
    if(iit==bit->end()){
        moveToNextBasicBlock();
    }
}

void ComplexMutator::moveToNextReplaceableInst(){
    moveToNextInst();
    while(!isReplaceable(&*iit))moveToNextInst();
    for(size_t i:whenMoveToNextInstFuncs){
        helpers[i]->whenMoveToNextInst();
    }
    currHelpersIt=helpers.begin();
}


void ComplexMutator::calcDomInst(){
    domInst.resize(pm->global_size());
    if(auto it=dtMap.find(fit->getName());it!=dtMap.end()){
        //add Parameters
        for(auto ait=fit->arg_begin();ait!=fit->arg_end();++ait){
            domInst.push_back(&*ait);
        }
        llvm::DominatorTree& DT=it->second;
        //add BasicBlocks before bitTmp
        for(auto bitTmp=fit->begin();bitTmp!=bit;++bitTmp){
            if(DT.dominates(&*bitTmp,&*bit)){
                for(auto iitTmp=bitTmp->begin();iitTmp!=bitTmp->end();++iitTmp){
                    domInst.push_back(&*iitTmp);
                }
            }
        }
        //add Instructions before iitTmp
        for(auto iitTmp=bit->begin();iitTmp!=iit;++iitTmp){
            if(DT.dominates(&*iitTmp,&*iit)){
                domInst.push_back(&*iitTmp);
            }
        }
    }
}

void ComplexMutator::randomMoveInstruction(llvm::Instruction* inst){
    if(inst->getNextNonDebugInstruction()->isTerminator()){
    //if(Random::getRandomBool()){
        randomMoveInstructionForward(inst);
    }else if(inst!=&*(inst->getParent()->begin())){
        randomMoveInstructionBackward(inst);
    }else{
        if(Random::getRandomBool()){
            randomMoveInstructionForward(inst);
        }else{
            randomMoveInstructionBackward(inst);
        }
    }
}

void ComplexMutator::randomMoveInstructionForward(llvm::Instruction* inst){
    size_t pos=0,newPos;

    for(auto it=inst->getParent()->begin();&*it!=inst;++it,++pos);
    newPos=Random::getRandomUnsigned()%pos;
    //llvm::errs()<<"both pos: "<<pos<<' '<<newPos<<"\n";
    llvm::SmallVector<llvm::Instruction*> v;
    llvm::SmallVector<llvm::Value*> domBackup;
    llvm::Instruction* newPosInst=inst;
    for(size_t i=pos;i!=newPos;--i){
        newPosInst=newPosInst->getPrevNonDebugInstruction();
        v.push_back(newPosInst);
        //remove Insts in current basic block
        domBackup.push_back(domInst.back());
        domInst.pop_back();
    }

    for(size_t i=0;i<inst->getNumOperands();++i){
        if(llvm::Value* op=inst->getOperand(i);std::find(v.begin(),v.end(),op)!=v.end()){
            setOperandRandomValue(inst,i);
            //inst->setOperand(i,getRandomValue(op->getType()));
        }
    }

    inst->moveBefore(newPosInst);

    //restore domInst
    while(!domBackup.empty()){
        domInst.push_back(domBackup.back());
        domBackup.pop_back();
    }
    fixAllValues();
}

void ComplexMutator::randomMoveInstructionBackward(llvm::Instruction* inst){
    size_t pos=0,newPos;

    for(auto it=inst->getParent()->begin();&*it!=inst;++it,++pos);
    newPos=Random::getRandomInt()%(inst->getParent()->size()-pos)+1+pos;    

    //need fix all insts used current inst in [pos,newPos]
    llvm::Instruction* newPosInst=inst;
    for(size_t i=pos;i!=newPos;++i){
        newPosInst=inst->getNextNonDebugInstruction();
        for(size_t op=0;op<newPosInst->getNumOperands();++op){
            if(llvm::Value* opP=newPosInst->getOperand(op);opP!=nullptr&&opP==inst){
                setOperandRandomValue(newPosInst,op);
                //newPosInst->setOperand(op,getRandomValue(opP->getType()));
            }
        }
    }

    inst->moveBefore(newPosInst);
    fixAllValues();
}

llvm::Value* ComplexMutator::getRandomConstant(llvm::Type* ty){
    if(ty->isIntegerTy()){
        return llvm::ConstantInt::get(ty,Random::getRandomUnsigned());
    }
    return llvm::UndefValue::get(ty);
}



llvm::Value* ComplexMutator::getRandomDominatedValue(llvm::Type* ty){
    if(ty!=nullptr&&!domInst.empty()){
        for(size_t i=0,pos=Random::getRandomUnsigned()%domInst.size();i<domInst.size();++i,++pos){
            if(pos==domInst.size())pos=0;
            if(domInst[pos]->getType()==ty){
                return &*vMap[domInst[pos]];
            }
        }
    }
    return nullptr;
}

llvm::Value* ComplexMutator::getRandomValueFromExtraFuncArgs(llvm::Type* ty){
    if(ty!=nullptr&&!extraFuncArgs.empty()){
        for(size_t i=0,pos=Random::getRandomUnsigned()%extraFuncArgs.size();i<extraFuncArgs.size();++i,++pos){
            if(pos==extraFuncArgs.size())pos=0;
            if(extraFuncArgs[pos]->getType()==ty){
                return &*vMap[extraFuncArgs[pos]];
            }
        }
    }
    return nullptr;
}

llvm::Value* ComplexMutator::getRandomValue(llvm::Type* ty){
    if(ty!=nullptr&&!valueFuncs.empty()){
        for(size_t i=0,pos=Random::getRandomUnsigned()%valueFuncs.size();i<valueFuncs.size();++i,++pos){
            if(pos==valueFuncs.size())pos=0;
            if(llvm::Value* result=(this->*valueFuncs[pos])(ty);result!=nullptr){
                return result;
            }
        }
    }
    return nullptr;
}

llvm::Value* ComplexMutator::getRandomPointerValue(llvm::Type* ty){
    if(ty->isPointerTy()){
        if(Random::getRandomUnsigned()%(1+valueFuncs.size())==0){
            return llvm::ConstantPointerNull::get((llvm::PointerType*)(ty));
        }
        return getRandomValue(ty);
    }
    return nullptr;
}

void ComplexMutator::addFunctionArguments(const llvm::SmallVector<llvm::Type*>& tys){
    if(!lazyUpdateInsts.empty()){
        llvm::SmallVector<llvm::Type*> tys;
        for(auto ait=tmpFit->arg_begin();ait!=tmpFit->arg_end();++ait){
            tys.push_back(ait->getType());
        }
        size_t oldArgSize=tmpFit->arg_size();
        llvm::ValueToValueMapTy VMap;
        LLVMUtil::insertFunctionArguments(&*tmpFit,tys,VMap);
        tmpIit=((llvm::Instruction*)&*VMap[&*tmpIit])->getIterator();
        tmpBit=((llvm::BasicBlock*)&*VMap[&*tmpBit])->getIterator();
        tmpFit=tmpBit->getParent()->getIterator();
        for(size_t i=0;i<lazyUpdateInsts.size();++i){
            lazyUpdateInsts[i]=(llvm::Instruction*)&*VMap[lazyUpdateInsts[i]];
        }
        for(auto it=vMap.begin();it!=vMap.end();++it){
            it->second=VMap[it->second];
        }
        for(size_t i=0;i<tys.size();++i){
            extraFuncArgs.push_back(tmpFit->getArg(i+oldArgSize));
        }
    }
}

void ComplexMutator::fixAllValues(){
    if(!lazyUpdateInsts.empty()){
        llvm::SmallVector<llvm::Type*> tys;
        for(size_t i=0;i<lazyUpdateInsts.size();++i){
            tys.push_back(lazyUpdateInsts[i]->getOperand(lazyUpdateArgPos[i])->getType());
        }
        addFunctionArguments(tys);
        for(size_t i=0;i<lazyUpdateInsts.size();++i){
            lazyUpdateInsts[i]->setOperand(lazyUpdateArgPos[i],getRandomValue(tys[i]));
        }
    }
}

void ComplexMutator::setOperandRandomValue(llvm::Instruction* inst,size_t pos){
    if(llvm::Type* ty=inst->getOperand(pos)->getType();ty!=nullptr){
        if(llvm::Value* val=getRandomValue(ty);val==nullptr||llvm::isa<llvm::UndefValue>(val)){
            lazyUpdateInsts.push_back(inst);
            lazyUpdateArgPos.push_back(pos);
        }else{
            inst->setOperand(pos,val);
        }
    }
}