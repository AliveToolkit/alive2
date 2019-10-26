#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace util {

struct AliveException {
  std::string msg;
  bool is_unsound;

public:
  AliveException(std::string &&msg, bool is_unsound)
    : msg(std::move(msg)), is_unsound(is_unsound) {}
};


class Errors {
  std::vector<std::pair<std::string, bool>> errs;

public:
  Errors() = default;
  Errors(const char *str, bool is_unsound);
  Errors(std::string &&str, bool is_unsound);
  Errors(AliveException &&e);

  void add(const char *str, bool is_unsound);
  void add(std::string &&str, bool is_unsound);
  void add(AliveException &&e);

  explicit operator bool() const { return !errs.empty(); }
  bool isUnsound() const;

  friend std::ostream& operator<<(std::ostream &os, const Errors &e);
};

}
