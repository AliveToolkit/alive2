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
  parent_pid = getpid();
  max_subprocesses = _max_subprocesses;
  return true;
}

pid_t unrestricted::limitedFork() {
  assert(getpid() == parent_pid);
  /*
   * reap zombies
   */
  int status;
  while (waitpid((pid_t)(-1), &status, WNOHANG) > 0) {
    if (WIFEXITED(status))
      --subprocesses;
  }
  /*
   * if we have too many children already, wait for some to exit
   */
  while (subprocesses >= max_subprocesses) {
    if (wait(&status) != -1 && WIFEXITED(status))
      --subprocesses;
  }
  std::fflush(nullptr);
  auto res = fork();
  if (res > 0)
    ++subprocesses;
  return res;
}

void unrestricted::finishChild() {
  assert(getpid() != parent_pid);
  exit(0);
}

void unrestricted::waitForAllChildren() {
  assert(getpid() == parent_pid);
  int status;
  while (wait(&status) != -1) {
    if (WIFEXITED(status))
      --subprocesses;
  }
  if (subprocesses != 0) {
    std::cerr << "Alive2: expected zero remaining children but we have "
              << subprocesses << "\n";
  }
}
