#include "SingleLineMutator.h"

const std::string SingleLineMutator::INST_INDENT="";

bool SingleLineMutator::openInputFile(const string& inputFile){
    std::ifstream fin(inputFile,std::ifstream::in);
    if(fin.is_open()){
        Parser parser;string str;
        for(int i=0;fin;i++){
            getline(fin,str);
            if(auto ptr=parser.parseSinleLine(str);ptr!=nullptr){
                if(ptr->isInstruction()){
                    updPos.insert(std::make_pair(i,std::move(std::unique_ptr<Instruction>(static_cast<Instruction*>(ptr.release())))));
                }else if(ptr->isFunctionDefinition()){
                    funcDefPos.insert(std::make_pair(i,std::move(std::unique_ptr<FunctionDefinition>(static_cast<FunctionDefinition*>(ptr.release())))));
                }else if(ptr->isFunctionAttributeGroupDefinition()){
                    updFuncGroupAttrDef.insert(std::make_pair(i,std::move(std::unique_ptr<FunctionAttributeGroupDefinition>(static_cast<FunctionAttributeGroupDefinition*>(ptr.release())))));
                }
            }
            testFile.push_back(str);
        }
        return true;
    }
    return false;
}

bool SingleLineMutator::init(){
    if(!updPos.empty()){
        for(const auto& p:funcDefPos){
            p.second->calcSupportedFunctionAttribute();
        }
        if(debug){
            std::cout<<"find "<<updPos.size()<<" locations to mutate instructions!\n";
        }
    }else{
        std::cerr<<"Cannot find any instruction to update!";
    }
    cur=updPos.begin();

    if(!funcDefPos.empty()){

        if(debug){
            std::cout<<"find "<<funcDefPos.size()<<" functions to mutate!\n";
        }
    }else{
        std::cerr<<"Cannot find any function to update!";
    }
    funcCur=funcDefPos.begin();
    
    return !funcDefPos.empty()||!funcDefPos.empty();
}

void SingleLineMutator::generateTest(const string& outputFile){
    if(debug){
        std::cout<<"writing to file: "<<outputFile<<"\n";
    }
    std::ofstream fout(outputFile,std::ofstream::out);
    if(cur==updPos.end()&&funcCur==funcDefPos.end()){
        cur=updPos.begin();
        funcCur=funcDefPos.begin();
    }
    for(int i=0,tmp;i<(int)testFile.size();++i){
        if(!changed&&cur!=updPos.end()&&i==cur->first){
            cur->second->mutate();
            if(debug){
                cur->second->print(std::cout<<"Upd line: ")<<"\n";
            }
            cur->second->print(fout<<INST_INDENT)<<"\n";
            ++cur;
            if(updPos.find(i)->second->isGEPInstruction()){
                tmp=cur->first;
                updPos.erase(i);
                cur=updPos.find(tmp);
            }
            changed=true;
        }else if(!changed&&funcCur!=funcDefPos.end()&&i==funcCur->first){
            funcCur->second->mutate();
            if(debug){
                funcCur->second->print(std::cout<<"Upd line: ")<<"\n";
            }
            const auto& funcAttr=funcCur->second->getFuncAttr();
            for(auto& p:funcAttr){
                if(p->isFunctionAttributeGroup()){
                    funcGroupAttrNames.insert(p->getName());
                }
            }
            funcDefPos.begin()->second->print(fout)<<"\n";
            ++funcCur;
            changed=funcChanged=true;
        }else{
            if(funcChanged){
                auto it=updFuncGroupAttrDef.find(i);
                if(it!=updFuncGroupAttrDef.end()&&funcGroupAttrNames.find(it->second->getName())!=funcGroupAttrNames.end()){
                    it->second->print(fout)<<"\n";
                }else{
                    fout<<testFile[i]<<"\n";
                }
            }else{
                fout<<testFile[i]<<"\n";
            }
        }
    }
    fout.close();
    changed=funcChanged=false;
    funcGroupAttrNames.clear();
}
