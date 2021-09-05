#include "ComplexMutator.h"

bool ComplexMutator::openInputFile(const std::string &InputFilename){
    auto MB =ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(InputFilename)));
    llvm::SMDiagnostic Diag;
    pm = getLazyIRModule(std::move(MB), Diag, context,
                            /*ShouldLazyLoadMetadata=*/true);
    if (!pm) {
        Diag.print("", llvm::errs(), false);
        return false;
    }
    ExitOnErr(pm->materializeAll());
    return true;
}
  
bool ComplexMutator::init(){
    //return true;
    for(fit=pm->begin();fit!=pm->end();++fit){
        for(bit=fit->begin();bit!=fit->end();++bit)
            for(iit=bit->begin();iit!=bit->end();++iit){
                if(isReplaceable(&*iit)){
                    return true;
                }
            }
    }
    return false;
}

void ComplexMutator::generateTest(const std::string& outputFileName){
    //return;
    restoreBackUp();
    for(auto it=iit->op_begin();it!=iit->op_end();++it){
        instArgs.push_back(it->get());
    }
    llvm::DominatorTree DT(*fit);
    iit->print(llvm::errs());
    updatedInst=&*iit;
    llvm::errs()<<"\nDT info"<<DT.dominates(&*(fit->getFunction().begin()->begin()),updatedInst);
    llvm::errs()<<"\n";
    std::error_code ec;
    llvm::raw_fd_ostream fout(outputFileName,ec);
    fout<<*pm;
    fout.close();
    //need to use functions in llvm-stress to generate a instruction randomly.
    
    moveToNextReplaceableInst();
}
void ComplexMutator::restoreBackUp(){
    if(updatedInst!=nullptr){
        for(size_t i=0;i<instArgs.size();++i){
            updatedInst->setOperand(i,instArgs[i]);
        }
        for(auto inst:newAdded){
            inst->eraseFromParent();
        }
        instArgs.clear();
        newAdded.clear();
        updatedInst=nullptr;
    }
}

bool ComplexMutator::isReplaceable(llvm::Instruction* inst){
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
}

void ComplexMutator::moveToNextBasicBlock(){
    ++bit;
    if(bit==fit->end()){
        moveToNextFuction();
    }else{
        iit=bit->begin();
    }
}

void ComplexMutator::moveToNextInst(){
    ++iit;
    if(iit==bit->end()){
        moveToNextBasicBlock();
    }
}

void ComplexMutator::moveToNextReplaceableInst(){
    moveToNextInst();
    while(!isReplaceable(&*iit))moveToNextInst();
}
