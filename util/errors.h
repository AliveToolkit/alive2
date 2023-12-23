#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>
#include <set>
#include <string>
#include <utility>

namespace util {

struct AliveException {
  std::string msg;
  bool is_unsound;

public:
  AliveException(std::string &&msg, bool is_unsound)
    : msg(std::move(msg)), is_unsound(is_unsound) {}
};


class Errors {
  std::set<std::pair<std::string, bool>> errs;
  std::set<std::string> warnings;

public:
  Errors() = default;
  Errors(const char *str, bool is_unsound);
  Errors(std::string &&str, bool is_unsound);
  Errors(AliveException &&e);

  void add(const char *str, bool is_unsound);
  void add(std::string &&str, bool is_unsound);
  void add(AliveException &&e);
  void addWarning(const char *str);

  explicit operator bool() const { return !errs.empty(); }
  bool isUnsound() const;
  bool hasWarnings() const { return !warnings.empty(); }

  friend std::ostream& operator<<(std::ostream &os, const Errors &e);
  void printWarnings(std::ostream &os) const;
};

}
