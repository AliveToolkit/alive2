// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#include "util/parallel.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

bool unrestricted::init(int _max_subprocesses) {
  parallel::init(_max_subprocesses);
  return true;
}

std::tuple<pid_t, std::ostream *, int> unrestricted::limitedFork() {
  return doFork();
}

void unrestricted::finishChild() {
  writeToParent();
  exit(0);
}

void unrestricted::waitForAllChildren() {
  parallel::waitForAllChildren();
}
