#include "singleLine.h"
#include "attributes.h"

const string FunctionAttributeGroupDefinition::ATTRIBUTES_PREFIX("attributes");
std::map<string,std::pair<bool,std::vector<std::unique_ptr<FunctionAttribute>>>> FunctionAttributeGroupDefinition::groupAttrs;

FunctionAttributeGroupDefinition::FunctionAttributeGroupDefinition(const string& name,std::vector<std::unique_ptr<FunctionAttribute>>& attrs,const string& comment):name(name),SingleLine(comment){
    std::vector<std::unique_ptr<FunctionAttribute>> result;
    bool hasNoFree=false;
    for(auto& attr:attrs){
        if(attr->hasSupportedFunctionAttribute()){
            hasNoFree=true;
        }else{
            result.push_back(std::move(attr));
        }
    }
    groupAttrs.insert(std::make_pair(name,std::make_pair(hasNoFree,std::move(result))));
}

std::ostream& FunctionAttributeGroupDefinition::print(std::ostream& os)const {
    os<<"attributes "<<name<<" = { ";
    for(const auto& attr:groupAttrs.find(name)->second.second){
        attr->print(os)<<" ";
    }
    return os<<"}"<<comment;
}

string FunctionAttributeGroupDefinition::toString()const {
    string result;
    result="attributes "+name+" = { ";
    for(const auto& attr:groupAttrs.find(name)->second.second){
        result+=(attr->toString()+" ");
    }
    result+="}";
    return result+comment;
}    

string InvalidParameter::toString()const{
    return type;
}
std::ostream& InvalidParameter::print(std::ostream& os)const{
    return os<<type;
}

void InvalidParameter::mutate(){

}

ValidParameter::ValidParameter(const string& type,std::vector<std::unique_ptr<ParameterAttribute>>& paramAttr,
        const string& name):Parameter(type),name(name){
    std::unique_ptr<ParameterAttribute> deferenceable=nullptr;
    supportedParameterAttribute=0;
    for(auto& p:paramAttr){
        if(int flag=p->getSupportedParameterAttribute();flag!=0){
            if(flag&1){
                deferenceable=std::move(p);
            }else if(flag&2){
                supportedParameterAttribute|=2;
            }
        }else{
            (this->paramAttr).push_back(std::move(p));
        }
    }
    if(deferenceable){
        supportedParameterAttribute|=1;
        this->paramAttr.push_back(std::move(deferenceable));
    }
}
string ValidParameter::toString()const{
    string result=type+" ";
    if(supportedParameterAttribute&2){
        result+="nocapture ";
    }
    for(auto& p:paramAttr){
        result+=p->toString()+" ";
    }
    return result+name;
}

std::ostream& ValidParameter::print(std::ostream& os)const{
    os<<type<<" ";
    if(supportedParameterAttribute&2){
        os<<"nocapture ";
    }
    for(auto& p:paramAttr){
        p->print(os)<<" ";
    }
    return os<<name;
}

void ValidParameter::mutate(){
    if(type.back()=='*'){
        supportedParameterAttribute^=2;
    }
    if(Random::getRandomBool()){
        if(supportedParameterAttribute&1){
            paramAttr.pop_back();
            supportedParameterAttribute^=1;
        }else if(type.back()=='*'){
            paramAttr.push_back(
                std::make_unique<ParameterAttribute>("dereferenceable",
                std::to_string(1<<(Random::getRandomInt()%4))));
            supportedParameterAttribute^=1;
        }
    }
}

const string FunctionDefinition::FUNCTION_PREFIX("define");

FunctionDefinition::FunctionDefinition(const string& beforeNameStr,const string& functionName,
                    std::vector<std::unique_ptr<Parameter>>& para,
                    const string& middleStr,std::vector<std::unique_ptr<FunctionAttribute>>& funcAttr,
                    const string& afterAttrStr,const string& comment):
            beforeNameStr(beforeNameStr),functionName(functionName),para(std::move(para)),middleStr(middleStr),
            afterAttrStr(afterAttrStr),SingleLine(comment){
                for(int i=0;i<funcAttr.size();++i){
                    if(!(funcAttr[i]->isSingleFunctionAttribute()&&funcAttr[i]->hasSupportedFunctionAttribute())){
                        (this->funcAttr).push_back(std::move(funcAttr[i]));
                    }else{
                        hasNoFree=true;
                    }
                }
            }

string FunctionDefinition::toString()const{
    string result=beforeNameStr+" "+functionName+"("+(para.empty()?"":para[0]->toString());
    for(size_t i=1;i<para.size();++i){
        result+=(", "+para[i]->toString());
    }
    result+=(") "+(middleStr.empty()?"":(middleStr+" "))+(hasNoFree?"nofree ":""));
    for(auto& p:funcAttr){
        result+=(p->toString()+" ");
    }
    return result+afterAttrStr+comment;
}

std::ostream& FunctionDefinition::print(std::ostream& os)const{
    os<<beforeNameStr<<" "<<functionName<<"(";
    if(!para.empty()){
        para[0]->print(os);
    }
    for(size_t i=1;i<para.size();++i){
        para[i]->print(os<<", ");
    }
    os<<") ";
    if(!middleStr.empty()){
        os<<middleStr<<" ";
    }
    os<<(hasNoFree?"nofree ":"");
    for(auto& p:funcAttr){
        p->print(os)<<" ";
    }
    return os<<afterAttrStr<<comment;
}

void FunctionDefinition::mutate(){
    hasNoFree=!hasNoFree;
    for(auto& p:para){
        p->mutate();
    }
}

void FunctionDefinition::calcSupportedFunctionAttribute(){
    for(auto& p:funcAttr){
        hasNoFree|=p->hasSupportedFunctionAttribute();
    }
}