// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/errors.h"

using namespace std;

namespace util {

Errors::Errors(const char *str) {
  add(str);
}

Errors::Errors(string &&str) {
  add(move(str));
}

void Errors::add(const char *str) {
  errs.emplace_back(str);
}

void Errors::add(string &&str) {
  errs.emplace_back(str);
}

bool Errors::isTimeout() const {
  return errs.size() == 1 && errs[0] == "Timeout";
}

bool Errors::isOOM() const {
  return errs.size() == 1 && errs[0] == "Out of memory; skipping function.";
}

bool Errors::isLoopyCFG() const {
  return errs.size() == 1 &&
         errs[0] == "Loops are not supported yet! Skipping function.";
}

ostream& operator<<(ostream &os, const Errors &errs) {
  for (auto &err : errs.errs) {
    os << "ERROR: " << err << '\n';
  }
  return os;
}

}
