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
        cur=updPos.begin();
        
    }else{
        std::cerr<<"Cannot find any instruction to update!";
    }

    if(!funcDefPos.empty()){
        funcCur=funcDefPos.begin();
    }else{
        std::cerr<<"Cannot find any function to update!";
    }
    
    return !funcDefPos.empty()&&!updPos.empty();
}

void SingleLineMutator::generateTest(const string& outputFile){
    if(debug){
        std::cout<<"writing to file: "<<outputFile<<"\n";
    }
    std::ofstream fout(outputFile,std::ofstream::out);
    for(int i=0,tmp;i<testFile.size();++i){
        if(!changed&&i==cur->first){
            cur->second->mutate();
            if(debug){
                cur->second->print(std::cout<<"Upd line: ")<<"\n";
            }
            cur->second->print(fout<<INST_INDENT)<<"\n";
            ++cur;
            if(cur==updPos.end())cur=updPos.begin();
            if(updPos.find(i)->second->isGEPInstruction()){
                tmp=cur->first;
                updPos.erase(i);
                cur=updPos.find(tmp);
            }
            changed=true;
        }else if(!changed&&!funcDefPos.empty()&&i==funcCur->first){
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
            if(funcCur==funcDefPos.end()){{
                funcCur=funcDefPos.begin();
            }}
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