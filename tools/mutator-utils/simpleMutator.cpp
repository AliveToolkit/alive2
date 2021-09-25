#include "simpleMutator.h"

void BinaryInstructionMutant::resetFastMathFlags(llvm::BinaryOperator* inst){
    if(llvm::isa<llvm::FPMathOperator>(inst)){
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

void BinaryInstructionMutant::resetNUWNSWFlags(llvm::BinaryOperator* inst){
    if(llvm::isa<llvm::OverflowingBinaryOperator>(inst)){
        inst->setHasNoSignedWrap(Random::getRandomBool());
        inst->setHasNoUnsignedWrap(Random::getRandomBool());
    }
}

void BinaryInstructionMutant::resetExactFlag(llvm::BinaryOperator* inst){
    inst->setIsExact(Random::getRandomBool());
}

const std::vector<std::function<void(llvm::BinaryOperator*)>> BinaryInstructionMutant::flagFunctions({BinaryInstructionMutant::doNothing,
                                                    BinaryInstructionMutant::resetNUWNSWFlags,
                                                    BinaryInstructionMutant::resetFastMathFlags,
                                                    BinaryInstructionMutant::resetExactFlag,
                                                    BinaryInstructionMutant::resetNUWNSWFlags,
                                                    BinaryInstructionMutant::resetExactFlag,
                                                    BinaryInstructionMutant::doNothing});

#define Ops llvm::Instruction::BinaryOps
const std::unordered_map<llvm::Instruction::BinaryOps,int> BinaryInstructionMutant::operToIndex({{Ops::URem,0},{Ops::SRem,0},
                                                    {Ops::Add,1},{Ops::Sub,1},{Ops::Mul,1},
                                                    {Ops::FAdd,2},{Ops::FSub,2},{Ops::FMul,2},{Ops::FDiv,2},{Ops::FRem,2},
                                                    {Ops::UDiv,3},{Ops::SDiv,3},
                                                    {Ops::Shl,4},
                                                    {Ops::LShr,5},{Ops::AShr,5},
                                                    {Ops::And,6},{Ops::Or,6},{Ops::Xor,6}});

const std::vector<std::vector<llvm::Instruction::BinaryOps>> BinaryInstructionMutant::indexToOperSet({{Ops::URem,Ops::SRem,},
                                                    {Ops::Add,Ops::Sub,Ops::Mul},
                                                    {Ops::FAdd,Ops::FSub,Ops::FMul,Ops::FDiv,Ops::FRem},
                                                    {Ops::UDiv,Ops::SDiv},
                                                    {Ops::Shl},
                                                    {Ops::LShr,Ops::AShr},
                                                    {Ops::And,Ops::Or,Ops::Xor}});
#undef attr

bool Mutator::openInputFile(const string& inputFile){
    auto MB =ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(inputFile)));
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
bool SimpleMutator::init(){
    int isBoring=0;
    for(auto fit=pm->begin();fit!=pm->end();++fit)
        if(!fit->isDeclaration()){
            mutants.push_back(std::make_pair(std::make_unique<FunctionDefinitionMutant>(&*fit),fit->getName()));
            if(mutants.back().first->isBoring()){
                ++isBoring;
            }
            for(llvm::inst_iterator iit=llvm::inst_begin(*fit),iitEnd=llvm::inst_end(*fit);iit!=iitEnd;++iit){
                if(llvm::isa<llvm::BinaryOperator>(&*iit)){
                    llvm::BinaryOperator* ptr=(llvm::BinaryOperator*)&*iit;
                    if(ptr->getOperand(0)->getType()->isVectorTy()){
                        continue;
                    }
                    mutants.push_back(std::make_pair(std::make_unique<BinaryInstructionMutant>(ptr),fit->getName()));
                    if(mutants.back().first->isBoring()){
                        ++isBoring;
                    }       
                }else if(llvm::isa<llvm::GetElementPtrInst>(&*iit)){
                    mutants.push_back(std::make_pair(std::make_unique<GEPInstructionMutant>((llvm::GetElementPtrInst*)&*iit),fit->getName()));
                    if(mutants.back().first->isBoring()){
                        ++isBoring;
                    }       
                }
        }
    }
    it=mutants.begin();
    return isBoring!=(int)mutants.size();
}

void SimpleMutator::mutateModule(const string& outputFileName){
    it->first->restoreMutate();
    if(isFirstRun&&it->first->isBoring()){
        it=mutants.erase(it);
    }else{
        ++it;
    }
    if(it==mutants.end()){
        it=mutants.begin();
        isFirstRun=false;
    }
    it->first->mutate();
    if(debug){
        it->first->print();
        std::error_code ec;
        llvm::raw_fd_ostream fout(outputFileName,ec);
        fout<<*pm;
        fout.close();
        llvm::errs()<<"file wrote to "<<outputFileName<<"\n";
    }
}
