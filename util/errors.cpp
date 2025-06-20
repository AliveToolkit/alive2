// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/errors.h"

using namespace std;

namespace util {

Errors::Errors(const char *str, bool is_unsound) {
  add(str, is_unsound);
}

Errors::Errors(string &&str, bool is_unsound) {
  add(std::move(str), is_unsound);
}

Errors::Errors(AliveException &&e) {
  add(std::move(e));
}

void Errors::add(const char *str, bool is_unsound) {
  add(string(str), is_unsound);
}

void Errors::add(string &&str, bool is_unsound) {
  if (is_unsound)
    errs.clear();
  errs.emplace(std::move(str), is_unsound);
}

void Errors::add(AliveException &&e) {
  add(std::move(e.msg), e.is_unsound);
}

void Errors::addWarning(const char *str) {
  warnings.emplace(str);
}

void Errors::clear() {
  errs.clear();
  warnings.clear();
}

bool Errors::isUnsound() const {
  for (auto &[msg, unsound] : errs) {
    if (unsound)
      return true;
  }
  return false;
}

ostream& operator<<(ostream &os, const Errors &errs) {
  for (auto &[msg, unsound] : errs.errs) {
    os << "ERROR: " << msg << '\n';
  }
  return os;
}

void Errors::printWarnings(std::ostream &os) const {
  for (auto &str : warnings) {
    os << "\n****************************************\n"
          "WARNING: " << str
       << "\n****************************************\n\n";
  }
}

}
