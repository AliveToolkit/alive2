// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/errors.h"

using namespace std;

namespace util {

Errors::Errors(const char *str, bool is_unsound) {
  add(str, is_unsound);
}

Errors::Errors(string &&str, bool is_unsound) {
  add(move(str), is_unsound);
}

Errors::Errors(AliveException &&e) {
  add(move(e));
}

void Errors::add(const char *str, bool is_unsound) {
  errs.emplace_back(str, is_unsound);
}

void Errors::add(string &&str, bool is_unsound) {
  errs.emplace_back(move(str), is_unsound);
}

void Errors::add(AliveException &&e) {
  add(move(e.msg), e.is_unsound);
}

bool Errors::isUnsound() const {
  for (auto &[msg, unsound] : errs) {
    (void)msg;
    if (unsound)
      return true;
  }
  return false;
}

ostream& operator<<(ostream &os, const Errors &errs) {
  for (auto &[msg, unsound] : errs.errs) {
    (void)unsound;
    os << "ERROR: " << msg << '\n';
  }
  return os;
}

}
