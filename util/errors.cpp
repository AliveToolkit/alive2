// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/errors.h"

using namespace std;

namespace util {

void Errors::add(string &&str) {
  errs.emplace_back(str);
}

ostream& operator<<(ostream &os, const Errors &errs) {
  for (auto &err : errs.errs) {
    os << "ERROR: " << err << '\n';
  }
  return os;
}

}
