#pragma once
#include "util.h"
#include <iostream>
#include <cstring>
#include <set>
#include <functional>
#include <numeric>
#include <map>

class Attribute{
protected:
    string name;
public:
    Attribute(const string& name):name(name){};
    const string& getName()const{return name;}
    virtual std::ostream& print(std::ostream& os)const=0;
    virtual void mutate(){};
    virtual string toString()const=0;
    virtual ~Attribute(){};
};

/*
 * 0x1 -> dereferenceable(<n>)
 * 0x2 -> nocapture
*/
class ParameterAttribute:public Attribute{
protected:
    string param;
    int supportedAttribute;
public:
    ParameterAttribute(const string& name,const string& param):Attribute(name),param(param){
        supportedAttribute=0;
        if(name=="dereferenceable"){
            supportedAttribute|=1;
        }else if(name=="nocapture"){
            supportedAttribute|=2;
        }
    };
    int getSupportedParameterAttribute()const{return supportedAttribute;}
    virtual std::ostream& print(std::ostream& os)const override{
        os<<name;
        if(!param.empty()){
            if(name=="align"){
                os<<" "<<param;
            }else{
                os<<"("<<param<<")";
            }
        }
        return os;
    }

    virtual string toString()const override{
        string result=name;
        if(!param.empty()){
            if(name=="align"){
                result+=" "+param;
            }else{
                result+="("+param+")";
            }
        }
        return result;
    }    

    virtual void mutate()override{
        if(supportedAttribute&1){
            param=std::to_string(1<<(Random::getRandomInt()%4));
        }
    }
};

class FunctionAttribute:public Attribute{
public:
    FunctionAttribute(const string& name):Attribute(name){};    
    virtual bool isSingleFunctionAttribute()const{return false;}
    virtual bool isFunctionAttributeGroup()const{return false;}
    virtual bool hasSupportedFunctionAttribute()const=0;
    virtual ~FunctionAttribute(){};
};

class SingleFunctionAttribute:public FunctionAttribute{
public:
    SingleFunctionAttribute(const string& name):FunctionAttribute(name){};
    virtual std::ostream& print(std::ostream& os)const override{
        return os<<name;
    }    
    virtual string toString()const override{
        return name;
    }
    virtual bool isSingleFunctionAttribute()const override{
        return true;
    }
    virtual bool hasSupportedFunctionAttribute()const override{
        return name=="nofree";
    }
};

class FunctionAttributeGroup:public FunctionAttribute{
public:
    FunctionAttributeGroup(const string& name):FunctionAttribute(name){};
    virtual std::ostream& print(std::ostream& os)const override{
        return os<<name;
    }    
    virtual string toString()const override{
        return name;
    }
    virtual bool isFunctionAttributeGroup()const override{
        return true;
    }
    virtual bool hasSupportedFunctionAttribute()const override;
};
