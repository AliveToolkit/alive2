#include "parser.h"

bool Parser::isIdentifier(const string& str){
    return !str.empty()&&(isNamedValues(str)||isUnnamedValues(str)||isConstant(str));
}   

bool Parser::isNamedValues(const string& str){
    auto prefix=[](char c){return c=='@'||c=='%';};
    auto prefix2=[](char c){return isalpha(c)||c=='$'||c=='.'||c=='_';};
    auto remains=[](char c){return isalnum(c)||c=='$'||c=='.'||c=='_';};
    return str.size()>=2&&prefix(str[0])&&prefix2(str[1])&&std::all_of(str.begin()+2,str.end(),remains);
}

bool Parser::isUnnamedValues(const string& str){
    return str.size()>=2&&((str[0]=='@'||str[0]=='%')&&std::all_of(str.begin()+1,str.end(),isdigit));
}

bool Parser::isConstant(const string& str){
    return Util::isInteger(str)||Util::isFloatDecimal(str)||Util::isFloatExponential(str);
}

bool Parser::isSupportBinaryOperator(const string& str){
    return operToFlagIndex.find(str)!=operToFlagIndex.end();
}

bool Parser::isFunctionGroupID(const string& str){
    return str.size()>=2&&str[0]=='#'&&Util::isInteger(str.substr(1));
}

bool Parser::isUnnamedAddr(const string& str){
    return "unnamed_addr"==str||"local_unnamed_addr"==str;
}

bool Parser::isAddrSpace(const string& str){
    return str.size()>11&&Util::isPrefix(str,"addrspace")&&Util::isInteger(string(str.begin()+10,str.end()-1));
}

std::pair<string,string> Parser::removeComment(const std::string& str){
    string tmp,comment;
    if(size_t pos=str.find(';');pos!=string::npos){
        comment=str.substr(pos);
        tmp=string(str.begin(),str.begin()+pos);        
    }else
        tmp=str;    
    return std::make_pair(tmp,comment);
}

unique_ptr<Instruction> Parser::parseInstruction(const vector<string>& tokens,const string& comment){
    if(unique_ptr<Instruction> ptr=parseBinaryInstruction(tokens,comment);ptr!=nullptr)
        return ptr;
    if(unique_ptr<Instruction> ptr=parseGEPInstruction(tokens,comment);ptr!=nullptr)
        return ptr;
    return nullptr;
}

unique_ptr<Instruction> Parser::parseInstruction(const string& str){
    auto [inst,comment] =removeComment(str);
    vector<string> tokens=Util::splitWithToken(Util::trim(inst));
    return parseInstruction(tokens,comment);
}

unique_ptr<BinaryInstruction> Parser::parseBinaryInstruction(const vector<string>& tokens,const string& comment){
    if(tokens.size()>=6&&isIdentifier(tokens[0])&&tokens[1]=="="&&isSupportBinaryOperator(tokens[2])){
        const string& op2=tokens.back();
        const string& op1=tokens[tokens.size()-2];
        vector<string> flags;
        for(int i=3;i<(int)tokens.size()-3;++i){
            flags.push_back(tokens[i]);
        }
        return std::make_unique<BinaryInstruction>(tokens[0],tokens[2],
                                                    flags,tokens[tokens.size()-3],op1.substr(0,op1.size()-1),op2,
                                                    operToFlagIndex.find(tokens[2])->second,comment);
    }
    return nullptr;
}


unique_ptr<GEPInstruction> Parser::parseGEPInstruction(const vector<string>& tokens,const string& comment){
    if(tokens.size()>=4&&isIdentifier(tokens[0])&&tokens[1]=="="&&GEPInstruction::INSTRUCTION==tokens[2]){
        bool hasInbounds=(tokens[3]==GEPInstruction::FLAG);
        return std::make_unique<GEPInstruction>(tokens[0],
                    std::accumulate(tokens.begin()+(hasInbounds?4:3),tokens.end(),string(""),[](const string& a,const string& b){return a+b+" ";}),
                    comment,hasInbounds);
    }
    return nullptr;
}

unique_ptr<BinaryInstruction> Parser::parseBinaryInstruction(const string& str){
    auto [inst,comment] =removeComment(str);
    vector<string> v=Util::splitWithToken(Util::trim(inst));
    return parseBinaryInstruction(v,comment);
}

unique_ptr<GEPInstruction> Parser::parseGEPInstruction(const string& str){
    auto [inst,comment] =removeComment(str);
    vector<string> v=Util::splitWithToken(Util::trim(inst));
    return parseGEPInstruction(v,comment);
}

unique_ptr<FunctionAttribute> Parser::parseFunctionAttribute(const std::string& str){
    if(isFunctionGroupID(str)){
        return std::make_unique<FunctionAttributeGroup>(str);
    }
    return std::make_unique<SingleFunctionAttribute>(str);
}

unique_ptr<FunctionAttributeGroupDefinition> Parser::parseFunctionAttributeGroupDefinition(const vector<string>& tokens, const string& comment){
    if(tokens.size()>=5&&(FunctionAttributeGroupDefinition::ATTRIBUTES_PREFIX==tokens[0])&&("="==tokens[2])&&("{"==tokens[3])&&("}"==tokens.back())){
        const string& name=tokens[1];
        if(isFunctionGroupID(name)){
            std::vector<unique_ptr<FunctionAttribute>> attrs;
            for(int i=4;i<(int)tokens.size()-1;++i){
                if(unique_ptr<FunctionAttribute> ptr=parseFunctionAttribute(tokens[i]);ptr!=nullptr){
                    attrs.push_back(std::move(ptr));
                }else{
                    return nullptr;
                }
            }
            return std::make_unique<FunctionAttributeGroupDefinition>(name,attrs,comment);
        }
    }
    return nullptr;
}

std::tuple<string,string,string,string> Parser::getFunctionNameAndParameter(const string& str){
    size_t atPos=str.find('@');
    if(atPos!=string::npos){
        size_t leftParen=str.find('(',atPos);
        if(leftParen!=string::npos&&isNamedValues(str.substr(atPos,leftParen-atPos))){
            size_t rightParen=leftParen,cur=1;
            do{
                ++rightParen;
                if(str[rightParen]=='(')++cur;
                else if(str[rightParen]==')')--cur;
            }while(rightParen+1<str.size()&&cur!=0);
            return {Util::trim(str.substr(0,atPos)),
                    Util::trim(str.substr(atPos,leftParen-atPos)),
                    Util::trim(str.substr(leftParen+1,rightParen-leftParen-1)),
                    Util::trim(str.substr(rightParen+1))};
        }
    }
    return {"","","",""};
}

static std::deque<string> splitFunctionAttribute(const string& str){
    std::deque<std::string> tmpRes;
    std::vector<int> pos(1,-1);
    for(int i=0,neasted=0;i<(int)str.size();++i){
        if(str[i]=='(')++neasted;
        else if(str[i]==')')--neasted;
        else if(neasted==0&&isblank(str[i]))pos.push_back(i);
    }
    pos.push_back(str.size());
    for(int i=1;i<(int)pos.size();++i){
        tmpRes.push_back(std::string(str.begin()+pos[i-1]+1,str.begin()+pos[i]));
    }
    return tmpRes;
}

std::tuple<string,std::vector<std::unique_ptr<FunctionAttribute>>,string> Parser::getFunctionAttribute(const string& str){
    std::deque<string> tokens=splitFunctionAttribute(str);
    if(!tokens.empty()&&tokens.back()=="{"){
        string middleStr;
        for(int i=0;!tokens.empty()&&i<2&&(isUnnamedAddr(tokens.front())||isAddrSpace(tokens.front()));++i){
            middleStr+=(tokens.front()+" ");
            tokens.pop_front();
        }
        if(!tokens.empty()){
            int lastDetectableAttr=tokens.size()-2;
            std::vector<std::unique_ptr<FunctionAttribute>> funcAttr;
            string afterAttrStr;
            //get last detectable attr and update the var;
            //current strategy is treating all tokens after middleStr as function attributes
            for(int i=0;i<=lastDetectableAttr;++i){
                funcAttr.push_back(std::move(parseFunctionAttribute(tokens[i])));
            }
            for(int i=lastDetectableAttr+1;i<(int)tokens.size();++i){
                afterAttrStr+=(tokens[i]+" ");
            }
            return {middleStr,std::move(funcAttr),afterAttrStr};
        }
    }
    return {"",std::vector<std::unique_ptr<FunctionAttribute>>(),""};
}

unique_ptr<ParameterAttribute> Parser::parseParameterAttribute(const string& str){
    if(size_t leftPara=str.find("(");leftPara!=string::npos){
        if(size_t rightPara=str.find_last_of(")");rightPara!=string::npos){
            return std::make_unique<ParameterAttribute>(str.substr(0,leftPara),str.substr(leftPara+1,rightPara-leftPara-1));
        }
        return nullptr;
    }else{
        return std::make_unique<ParameterAttribute>(str,"");
    }
}

unique_ptr<Parameter> Parser::parseParameter(const string& str){
    vector<string> tokens=Util::splitWithToken(str);
    if(!tokens.empty()){
        string type=tokens[0],name;
        std::vector<std::unique_ptr<ParameterAttribute>> paras;
        int hasName=0;
        if(tokens.size()>1){
            if(isNamedValues(tokens.back())||isUnnamedValues(tokens.back())){
                hasName=1;
                name=tokens.back();
            }
        }
        for(int i=1;i<(int)tokens.size()-hasName;){
            if(tokens[i]=="align"){
                if(i+1>=(int)tokens.size()-hasName){
                    return nullptr;
                }
                if(unique_ptr<ParameterAttribute> ptr=parseParameterAttribute(tokens[i]+"("+tokens[i+1]+")");ptr){
                    paras.push_back(std::move(ptr));
                    i+=2;
                }
            }else if(unique_ptr<ParameterAttribute> ptr=parseParameterAttribute(tokens[i]);ptr){
                paras.push_back(std::move(ptr));
                i++;
            }else{
                return std::make_unique<InvalidParameter>(str);
            }
        }
        return std::make_unique<ValidParameter>(type,paras,name);
    }
    return nullptr;
}

std::vector<unique_ptr<Parameter>> Parser::getFunctionParameter(const string& str){
    std::vector<unique_ptr<Parameter>> result;
    std::vector<string> tokens=Util::splitWithToken(str,",");
    for(const string& s:tokens){
        if(unique_ptr<Parameter> ptr=parseParameter(Util::trim(s));ptr){
            result.push_back(std::move(ptr));
        }
    }
    return result;
}


unique_ptr<FunctionDefinition> Parser::parseFunctionDefinition(const string& str,const string& comment){
    if(Util::isPrefix(str,FunctionDefinition::FUNCTION_PREFIX)){
        auto [beforeFunctionName,functionName,funcParaStr,rest]=getFunctionNameAndParameter(str);
        if(beforeFunctionName.empty()||functionName.empty()||funcParaStr.empty()||rest.empty())return nullptr;
        auto [middleStr,funcAttrs,afterAttrStr]=getFunctionAttribute(rest);
        if(afterAttrStr.empty())return nullptr;
        std::vector<unique_ptr<Parameter>> funcPara=std::move(getFunctionParameter(funcParaStr));
        return std::make_unique<FunctionDefinition>(beforeFunctionName,functionName,
                                                    funcPara,middleStr,
                                                    funcAttrs,afterAttrStr,comment);
    }
    return nullptr;
}

unique_ptr<SingleLine> Parser::parseSinleLine(const string& str){
    auto [define,comment]=removeComment(Util::trim(str));
    if(unique_ptr<FunctionDefinition> ptr=parseFunctionDefinition(define,comment);ptr!=nullptr){
        return ptr;
    }
    vector<string> tokens=Util::splitWithToken(define);
    if(unique_ptr<FunctionAttributeGroupDefinition> ptr=parseFunctionAttributeGroupDefinition(tokens,comment);ptr!=nullptr){
        return ptr;
    }
    if(unique_ptr<Instruction> ptr=parseInstruction(tokens,comment);ptr!=nullptr){
        return ptr;
    }
    return nullptr;
}
