#include "util.h"

std::random_device Random::rd;
std::uniform_int_distribution<int> Random::dist(0,2147483647u);

string Util::trim(const string& str){
    int ri=str.size()-1;
    while(ri>=0&&isblank(str[ri]))--ri;
    int le=0;
    while(le<ri&&isblank(str[le]))++le;
    return string(str.begin()+le,str.begin()+ri+1);
}

bool Util::isInteger(const string& str){
    return !str.empty()&&std::all_of(str.begin(),str.end(),isdigit);
}

bool Util::isFloatDecimal(const string& str){
    if(str.empty())return false;
    bool hasDot=false;
    for(const auto& c: str){
        if(!isdigit(c)){
            if(c=='.'){
                if(hasDot)return false;
                hasDot=true;
            }else{
                return false;
            }
        }
    }
    return true;
}

bool Util::isFloatExponential(const string& str){
    if(str.empty())return false;
    size_t pos=str.find('e');
    if(pos==string::npos||pos==str.size()-1)return false;
    int offset=0;
    if(str[pos+1]=='+'||str[pos+1]=='-'||isdigit(str[pos+1])){
        if(!isdigit(str[pos+1]))++offset;
        return isFloatDecimal(str.substr(0,pos))&&isInteger(str.substr(pos+1+offset));
    }else{
        return false;
    }
}

std::vector<string> Util::split(const string& origin,const string& delimiter){
    int startPos = 0,endPos = origin.find(delimiter);
    std::vector<string> res;
    while (endPos != string::npos) {
        res.push_back(origin.substr(startPos,endPos-startPos));
        startPos = endPos + delimiter.size();
        endPos = origin.find(delimiter, startPos);
    }
    res.push_back(origin.substr(startPos,endPos-startPos));
    return res;
}

std::vector<string> Util::splitWithToken(const std::string& str,const std::string& pattern){ 
    std::vector<std::string> tmpRes;
    std::vector<int> pos(1,-(int)(pattern.size()));
    for(int i=0,neasted=0;i<str.size();++i){
        if(str[i]=='<')++neasted;
        else if(str[i]=='>')--neasted;
        else if(neasted==0&&str.substr(i,pattern.size())==pattern)pos.push_back(i);
    }
    pos.push_back(str.size());
    for(int i=1;i<pos.size();++i){
        tmpRes.push_back(std::string(str.begin()+pos[i-1]+pattern.size(),str.begin()+pos[i]));
    }
    return tmpRes;
}

bool Util::isPrefix(const string& str,const string& prefix,int offset){
    if(str.size()>=prefix.size()+offset){
        for(int i=0;i<prefix.size();++i)
        if(str[i+offset]!=prefix[i])
            return false;
        return true;
    }
    return false;
}