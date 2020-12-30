// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/parallel.h"

bool null::init() {
  return true;
}

void null::getToken() {
}

void null::putToken() {
}

std::tuple<pid_t, std::ostream *, int> null::limitedFork() {
  static int nextPid = 0;
  ++nextPid;
  return {nextPid, nullptr, nextPid};
}

void null::finishChild(bool is_timeout) {}

void null::finishParent() {}
