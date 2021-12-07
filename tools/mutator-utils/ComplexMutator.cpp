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
    if(debug){
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
                return extraFuncArgs[pos];
            }
        }
    }
    return nullptr;
}

llvm::Value* ComplexMutator::getRandomValue(llvm::Type* ty){
    if(ty!=nullptr&&!valueFuncs.empty()){
        bool isUndef=false;
        for(size_t i=0,pos=Random::getRandomUnsigned()%valueFuncs.size();i<valueFuncs.size();++i,++pos){
            if(pos==valueFuncs.size())pos=0;
            if(llvm::Value* result=(this->*valueFuncs[pos])(ty);result!=nullptr){
                if(llvm::isa<llvm::UndefValue>(result)){
                    isUndef=true;
                    continue;
                }
                return result;
            }
        }
        if(isUndef){
            return llvm::UndefValue::get(ty);
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
        //llvm::SmallVector<llvm::Type*> tys;
        /*for(auto ait=tmpFit->arg_begin();ait!=tmpFit->arg_end();++ait){
            tys.push_back(ait->getType());
        }*/
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
        addFunctionArguments(lazyUpdateArgTys);
        for(size_t i=0;i<lazyUpdateInsts.size();++i){
            lazyUpdateInsts[i]->setOperand(lazyUpdateArgPos[i],getRandomValue(lazyUpdateArgTys[i]));
            /*llvm::errs()<<"AAAAAAAAAAAAAAAAAAAAAAA\n";
            lazyUpdateInsts[i]->getOperand(lazyUpdateArgPos[i])->print(llvm::errs());            
            llvm::errs()<<"\nAAAAAAAAAAAAAAAAAAAAAAA\n";
            llvm::errs()<<lazyUpdateInsts[i]->getParent()->getParent()->getName()<<"\n";*/
        }
        lazyUpdateArgTys.clear();
        lazyUpdateArgPos.clear();
        lazyUpdateInsts.clear();
    }
}

void ComplexMutator::setOperandRandomValue(llvm::Instruction* inst,size_t pos){
    if(llvm::Type* ty=inst->getOperand(pos)->getType();ty!=nullptr){
        if(llvm::Value* val=getRandomValue(ty);val==nullptr||llvm::isa<llvm::UndefValue>(val)){
            lazyUpdateInsts.push_back(inst);
            lazyUpdateArgPos.push_back(pos);
            lazyUpdateArgTys.push_back(inst->getOperand(pos)->getType());
            inst->setOperand(pos,llvm::UndefValue::get(ty));
        }else{
            inst->setOperand(pos,val);
        }
    }
}