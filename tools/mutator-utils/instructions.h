#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <numeric>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include "util.h"
#include "singleLine.h"

using std::string;
using std::vector;

extern const std::vector<string> operFlags[];
extern const std::unordered_map<string,int> operToFlagIndex;
extern const vector<vector<string>> FlagIndexToOper;


class Instruction:public SingleLine{
public:
    Instruction(const string& comment):SingleLine(comment){};
    virtual ~Instruction(){};
    virtual void mutate() override=0;
    virtual string toString()const override=0;
    virtual bool isGEPInstruction()const{return false;}
    virtual bool isBinaryInstruction()const{return false;}
    virtual bool isInstruction()const override{return true;}
    virtual std::ostream& print(std::ostream& os)const override=0;
};

class GEPInstruction:public Instruction{
    string result,rest;
    bool hasInbounds;
public:
    const static string INSTRUCTION;
    const static string FLAG;
    bool isGEPInstruction()const override{return true;}
    void resetFlags(){
        hasInbounds=!hasInbounds;
    }
    string toString()const override{
        return result +" = " +INSTRUCTION +(hasInbounds?" inbounds ":" ") + rest+" "+comment;
    }
    GEPInstruction(const string& result,const string& rest,const string& comment,bool hasInbounds):Instruction(comment),
            result(result),rest(rest),hasInbounds(hasInbounds){}
    ~GEPInstruction(){}
    void mutate() override{
        resetFlags();
    }
    virtual std::ostream& print(std::ostream& os)const override{
        os<<result<<" = "<<INSTRUCTION<<(hasInbounds? " inbounds " :" ")<<rest<<" "<<comment;
        return os;
    }
};

class BinaryInstruction:public Instruction{
    string result,oper, op1, op2, ty;
    vector<string> flags;
    int flagIndex;
public:
    const vector<string>& getFlags()const{return flags;};
    bool isBinaryInstruction()const override{return true;}
    void resetFlags(){
        if(!operFlags[flagIndex].empty()){
            vector<string> res;
            do{
                res.clear();
                for_each(operFlags[flagIndex].begin(),operFlags[flagIndex].end(),[&res](const string& str){if(Random::getRandomBool())res.push_back(str);});
            }while(res==flags);
            flags=std::move(res);
        }
    };
    void swapOperands(){
        swap(op1,op2);
    }
    void replaceConstant(){
        if(ty.size()>=2&&ty.front()=='i'&&(Util::isInteger(op1)||Util::isInteger(op2))){
            unsigned long long sz=std::max(std::stoi(ty.substr(1),nullptr)-1,1);
            if(Util::isInteger(op1))
                op1=std::to_string(Random::getRandomInt()%(1ull<<sz));
            if(Util::isInteger(op2))
                op2=std::to_string(Random::getRandomInt()%(1ull<<sz));
        }
    }
    void resetOperator(){
        if(FlagIndexToOper[flagIndex].size()>1){
            string res;
            do{
                res=FlagIndexToOper[flagIndex][abs(Random::getRandomInt())%FlagIndexToOper[flagIndex].size()];
            }while(res==oper);
            oper=std::move(res);
        }
    };
    ~BinaryInstruction(){};
    BinaryInstruction(const string& result, const string& oper,const vector<string>& flags,const string& ty,const string& op1,const string& op2,int flagIndex,const string& comment=""):Instruction(comment),
                    result(result),oper(oper),op1(op1),op2(op2),ty(ty),flags(flags),flagIndex(flagIndex){};
    string toString()const override{
        return result+" = "+oper+" "+std::accumulate(flags.begin(),flags.end(),string(),Util::stringAccumulateAdd)+ty+" "+op1+", "+op2+comment;
    };

    virtual std::ostream& print(std::ostream& os)const override{
        os<<result<<" = "<<oper<<" ";
        for(const string& flag:flags)
            os<<flag<<" ";
        os<<ty<<" "<<op1<<", "<<op2<<comment;
        return os;
    }

    void mutate() override{
        resetOperator();
        resetFlags();
        if(Random::getRandomBool())
            swapOperands();
        replaceConstant();
    }
};
