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
    static unsigned getRandomUnsigned(){return abs(dist(rd));}
    static bool getRandomBool(){return dist(rd)&1;}
};
