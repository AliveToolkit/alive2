#include "util.h"

std::random_device Random::rd;
std::uniform_int_distribution<int> Random::dist(0,2147483647u);
