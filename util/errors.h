#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>
#include <string>
#include <vector>

namespace util {

class Errors {
  std::vector<std::string> errs;

public:
  Errors() = default;
  Errors(const char *str);
  Errors(std::string &&str);
  void add(const char *str);
  void add(std::string &&str);
  explicit operator bool() const { return !errs.empty(); }
  bool isTimeout() const;
  bool isInvalidExpr() const;
  bool isOOM() const;
  bool isLoopyCFG() const; // FIXME

  friend std::ostream& operator<<(std::ostream &os, const Errors &e);
};

}
