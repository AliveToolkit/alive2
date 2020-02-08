#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <unordered_set>
#include <vector>

namespace util {

  using edgesTy = std::vector<std::unordered_set<unsigned>>;
  std::vector<unsigned> top_sort(const edgesTy &edges);

}
