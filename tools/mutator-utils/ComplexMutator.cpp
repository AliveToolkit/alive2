#include "ComplexMutator.h"

bool ComplexMutator::init(){
    for(auto funcIt=pm->begin();funcIt!=pm->end();++funcIt){
        for(auto ait=funcIt->arg_begin();ait!=funcIt->arg_end();++ait){
            if(ait->hasAttribute(llvm::Attribute::AttrKind::ImmArg)){
                filterSet.insert(funcIt->getName().str());
                break;
            }
        }
    }

    bool result=false;
    for(fit=pm->begin();fit!=pm->end();++fit){
        if(fit->isDeclaration()){
            continue;
        }
        for(bit=fit->begin();bit!=fit->end();++bit)
            for(iit=bit->begin();iit!=bit->end();++iit){
                if(isReplaceable(&*iit)){
                    result=true;
                    goto end;
                }
            }
    }
end:
    if(result){
        calcDomInst();
    }
    return result;
}

void ComplexMutator::mutateModule(const std::string& outputFileName){
    restoreBackUp();
    for(auto it=iit->op_begin();it!=iit->op_end();++it){
        instArgs.push_back(it->get());
    }
    if(debug){
        iit->print(llvm::errs());
    }
    updatedInst=&*iit;
    currFuncName=fit->getName().str();
    insertRandomBinaryInstruction(updatedInst);
    if(debug){
        bit->print(llvm::errs());
        llvm::errs()<<"\nDT info"<<DT.dominates(&*(fit->getFunction().begin()->begin()),updatedInst);
        llvm::errs()<<"\n";
        std::error_code ec;
        llvm::raw_fd_ostream fout(outputFileName,ec);
        fout<<*pm;
        fout.close();
        llvm::errs()<<"file wrote to "<<outputFileName<<"\n";
    }
    moveToNextReplaceableInst();
    while(newAdded.back()==&*iit)
        moveToNextReplaceableInst();
}

void ComplexMutator::restoreBackUp(){
    if(updatedInst!=nullptr){
        llvm::Instruction* newInst=&*iit;
        for(size_t i=0;i<instArgs.size();++i){
            updatedInst->setOperand(i,instArgs[i]);
        }
        for(auto inst:newAdded){
            inst->eraseFromParent();
            if(auto it=std::find(domInst.begin(),domInst.end(),inst);it!=domInst.end()){
                domInst.erase(it);
            }
        }
        instArgs.clear();
        newAdded.clear();
        updatedInst=nullptr;
        iit=decltype(bit->begin())(newInst);
    }
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
    DT=llvm::DominatorTree(*fit);
    bit=fit->begin();
    iit=bit->begin();
}

void ComplexMutator::moveToNextBasicBlock(){
    ++bit;
    if(bit==fit->end()){
        moveToNextFuction();
    }else{
        iit=bit->begin();
    }
    calcDomInst();
}

void ComplexMutator::moveToNextInst(){
    if(std::find(newAdded.begin(),newAdded.end(),&*iit)==newAdded.end()){
        domInst.push_back(&*iit);
    }
    ++iit;
    if(iit==bit->end()){
        moveToNextBasicBlock();
    }
}

void ComplexMutator::moveToNextReplaceableInst(){
    moveToNextInst();
    while(!isReplaceable(&*iit))moveToNextInst();
}


void ComplexMutator::calcDomInst(){
    domInst.clear();
    DT=llvm::DominatorTree(*fit);
    for(auto bitTmp=bit->getParent()->begin();bitTmp!=bit;++bitTmp){
        for(auto iitTmp=bitTmp->begin();iitTmp!=bitTmp->end();++iitTmp){
            if(DT.dominates(&*iitTmp,&*iit)){
                domInst.push_back(&*iitTmp);
            }
        }
    }
    for(auto iitTmp=bit->begin();iitTmp!=iit;++iitTmp){
        if(DT.dominates(&*iitTmp,&*iit)){
            domInst.push_back(&*iitTmp);
        }
    }
}

void ComplexMutator::insertRandomBinaryInstruction(llvm::Instruction* inst){
    size_t pos=Random::getRandomUnsigned()%inst->getNumOperands();
    llvm::Type* ty=nullptr;
    for(size_t i=0;i<inst->getNumOperands();++i,++pos){
        if(pos==inst->getNumOperands())pos=0;
        if(inst->getOperand(pos)->getType()->isIntegerTy()){
            ty=inst->getOperand(pos)->getType();
            break;
        }
    }
   /* llvm::errs()<<"dom size"<<domInst.size()<<"\n";
    for(const auto& x:domInst){
        x->print(llvm::errs());
        llvm::errs()<<"\n";
    }
    llvm::errs()<<"\n";*/
    llvm::Value* val1=getRandomValue(ty),*val2=getRandomValue(ty);
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
    newAdded.push_back(newInst);
    inst->setOperand(pos,newInst);
}

llvm::Constant* ComplexMutator::getRandomConstant(llvm::Type* ty){
    if(ty->isIntegerTy()){
        return llvm::ConstantInt::get(ty,Random::getRandomUnsigned());
    }
    return llvm::UndefValue::get(ty);
}

llvm::Value* ComplexMutator::getRandomValue(llvm::Type* ty){
    if(ty!=nullptr&&!domInst.empty()){
        for(size_t i=0,pos=Random::getRandomUnsigned()%domInst.size();i<domInst.size();++i,++pos){
            if(pos==domInst.size())pos=0;
            if(domInst[pos]->getType()==ty){
                return domInst[pos];
            }
        }
    }
    return getRandomConstant(ty);
}