// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/sort.h"
#include <algorithm>
#include <functional>

using namespace std;

namespace util {

// simple Tarjan topological sort ignoring loops
vector<unsigned> top_sort(const edgesTy &edges) {
  vector<unsigned> sorted;
  vector<unsigned char> marked;
  marked.resize(edges.size());

  function<void(unsigned)> visit = [&](unsigned v) {
    if (marked[v])
      return;
    marked[v] = true;

    for (auto child : edges[v]) {
      visit(child);
    }
    sorted.emplace_back(v);
  };

  for (unsigned i = 1, e = edges.size(); i < e; ++i)
    visit(i);
  if (!edges.empty())
    visit(0);

  reverse(sorted.begin(), sorted.end());
  return sorted;
}

}
