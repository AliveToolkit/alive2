// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/parallel.h"
#include <cstdlib>

bool unrestricted::init(int max_subprocesses) {
  return parallel::init(max_subprocesses);
}

std::tuple<pid_t, std::ostream *, int> unrestricted::limitedFork() {
  return doFork();
}

void unrestricted::finishChild() {
  writeToParent();
}

void unrestricted::waitForAllChildren() {
  parallel::waitForAllChildren();
}
