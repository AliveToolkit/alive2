#pragma once
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>

using std::string;

class FunctionAttribute;
class ParameterAttribute;

class SingleLine{
protected:
    string comment;
public:
    SingleLine(const string& comment):comment(comment){};
    virtual void mutate()=0;
    virtual string toString()const=0;
    virtual std::ostream& print(std::ostream& os)const=0;
    virtual bool shouldRandomMutate()const {return true;}
    virtual bool isInstruction()const{return false;}
    virtual bool isFunctionAttributeGroupDefinition()const{return  false;}
    virtual bool isFunctionDefinition()const{return false;}
    virtual ~SingleLine(){};
};

class FunctionAttributeGroupDefinition:public SingleLine{
protected:
    string name;    
public:
    const static string ATTRIBUTES_PREFIX;
    const string& getName()const{return name;}
    static std::map<string,std::pair<bool,std::vector<std::unique_ptr<FunctionAttribute>>>> groupAttrs;
    virtual bool shouldRandomMutate()const override{return false;}
    virtual bool isFunctionAttributeGroupDefinition()const override{return true;}
    FunctionAttributeGroupDefinition(const string& name,std::vector<std::unique_ptr<FunctionAttribute>>& attrs,const string& comment);
    virtual std::ostream& print(std::ostream& os)const override;
    virtual string toString()const override;
    virtual void mutate()override{};
};

class Parameter{
protected:
    string type;
public:
    Parameter(const string& type):type(type){};    
    virtual ~Parameter(){};
    virtual string toString()const =0;
    virtual std::ostream& print(std::ostream& os)const=0;
    virtual void mutate()=0;
};

class InvalidParameter:public Parameter{
public:
    InvalidParameter(const string& type):Parameter(type){};
    string toString()const override;
    std::ostream& print(std::ostream& os)const override;
    void mutate()override;
};

class ValidParameter:public Parameter{
protected:
    string name;
    std::vector<std::unique_ptr<ParameterAttribute>> paramAttr;
    int supportedParameterAttribute;
public:
    ValidParameter(const string& type,std::vector<std::unique_ptr<ParameterAttribute>>& paramAttr,
            const string& name);
    string toString()const override;
    std::ostream& print(std::ostream& os)const override;
    void mutate()override;
    bool hasSupportedParameterAttribute()const{return supportedParameterAttribute!=0;}
    int getSupportedParameterAttribute()const{return supportedParameterAttribute;}
};

class FunctionDefinition:public SingleLine{
protected:
    string functionName,beforeNameStr,middleStr,afterAttrStr;
    std::vector<std::unique_ptr<Parameter>> para;
    std::vector<std::unique_ptr<FunctionAttribute>> funcAttr;
    bool hasNoFree;
public:    
    const static string FUNCTION_PREFIX;
    virtual bool isFunctionDefinition()const override{return true;}
    FunctionDefinition(const string& beforeNameStr,const string& functionName,
                    std::vector<std::unique_ptr<Parameter>>& para,
                    const string& middleStr,std::vector<std::unique_ptr<FunctionAttribute>>& funcAttr,
                    const string& afterAttrStr,const string& comment);
    virtual string toString()const override;
    virtual std::ostream& print(std::ostream& os)const override;
    virtual void mutate()override;
    void calcSupportedFunctionAttribute();
    const std::vector<std::unique_ptr<FunctionAttribute>>& getFuncAttr()const{return funcAttr;}
};
