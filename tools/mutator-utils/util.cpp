#include "util.h"

std::random_device Random::rd;
std::uniform_int_distribution<int> Random::dist(0,2147483647u);
unsigned Random::seed(rd());
std::mt19937 Random::mt(Random::seed);

#define pm fit->getParent()
void ModuleIterator::moveToNextInstruction(){
    ++fit;
    ++functionDistance;
    if(fit==pm->end()){
        fit=pm->begin();
        functionDistance=0;
    }
    while(fit->isDeclaration()){
        ++fit;
        ++functionDistance;
        if(fit==pm->end()){
            fit=pm->begin();
            functionDistance=0;
        }
    }
    bit=fit->begin();
    basicBlockDistance=0;
    iit=bit->begin();
    instructionDistance=0;
}
#undef pm

void ModuleIterator::moveToNextBasicBlock(){
    ++bit;
    ++basicBlockDistance;
    if(bit==fit->end()){
        moveToNextFuction();
    }else{
        iit=bit->begin();
        instructionDistance=0;
    }
}

void ModuleIterator::moveToNextFuction(){
    ++iit;
    ++instructionDistance;
    if(iit==bit->end()){
        moveToNextBasicBlock();
    }
}