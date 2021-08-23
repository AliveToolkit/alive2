// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/unionfind.h"

using namespace std;

namespace util {

UnionFind::UnionFind(unsigned n) {
  for (unsigned i = 0; i < n; ++i) {
    id.push_back(i);
  }
}

unsigned UnionFind::mk() {
  unsigned n = id.size();
  id.push_back(n);
  return n;
}

unsigned UnionFind::find(unsigned i) {
  unsigned root = i;
  while (root != id[root])
    root = id[root];
  id[i] = root;
  return root;
}

unsigned UnionFind::merge(unsigned p, unsigned q) {
  unsigned i = find(p);
  unsigned j = find(q);
  id[i] = j;
  return j;
}

}
