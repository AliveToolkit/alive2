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
  static unsigned seed;
  static std::mt19937 mt;
  static std::uniform_int_distribution<int> dist;
  public:
    static int getRandomInt(){return dist(mt);}
    static unsigned getRandomUnsigned(){return abs(dist(mt));}
    static bool getRandomBool(){return dist(mt)&1;}
    static unsigned getSeed(){return seed;}
    static void setSeed(unsigned seed_){
      seed=seed_;
      mt=std::mt19937(seed);
    }
};
