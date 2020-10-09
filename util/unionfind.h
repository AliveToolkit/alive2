#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <vector>

namespace util {

class UnionFind final {
  std::vector<unsigned> id;
public:
  UnionFind(unsigned n);
  unsigned find(unsigned i);
  void merge(unsigned p, unsigned q);
};

}
