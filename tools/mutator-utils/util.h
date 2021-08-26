#pragma once
#include <string>
#include <algorithm>
#include <vector>
#include <memory>
#include <random>
#include <ctime>

using std::string;

class Random{
  static std::random_device rd;
  static std::uniform_int_distribution<int> dist;
  public:
    static int getRandomInt(){return dist(rd);}
    static bool getRandomBool(){return dist(rd)&1;}
};

class Util{
public:     
    static string trim(const string& str);
    static bool isInteger(const string& str);
    static bool isFloatDecimal(const string& str);
    static bool isFloatExponential(const string& str);
    static std::vector<string> split(const string& origin,const string& delimiter);
    static std::vector<string> splitWithToken(const std::string& str,const std::string& pattern=" ");
    static bool isPrefix(const string& str,const string& prefix,int offset=0);
    static string stringAccumulateAdd(const string& a,const string& b){return a+b+" ";}
};