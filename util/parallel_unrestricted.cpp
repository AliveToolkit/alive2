// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/parallel.h"

bool unrestricted::init() {
  return parallel::init();
}

std::tuple<pid_t, std::ostream *, int> unrestricted::limitedFork() {
  return parallel::limitedFork();
}

void unrestricted::finishChild(bool is_timeout) {
  parallel::finishChild(is_timeout);
}

void unrestricted::finishParent() {
  parallel::finishParent();
}
