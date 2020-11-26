// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#include "util/parallel.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

bool jobServer::init(int _max_subprocesses) {
  auto env = std::getenv("MAKEFLAGS");
  if (!env)
    return false;
  auto sub = strstr(env, "jobserver-auth="); // new GNU make
  if (!sub)
    sub = strstr(env, "jobserver-fds="); // old GNU make
  if (!sub)
    return false;
  int rfd, wfd;
  int res = sscanf(sub, "jobserver-auth=%d,%d", &rfd, &wfd);
  if (res != 2)
    res = sscanf(sub, "jobserver-fds=%d,%d", &rfd, &wfd);
  if (res != 2)
    return false;

  /*
   * this is a bit of hacky error protection for the very real
   * possibility that GNU Make closed the jobserver pipe fds when
   * exec'ing us, and then LLVM has already opened files corresponding
   * to these descriptors. here we simply make sure that both fds
   * correspond to pipe ends, which is at least somewhat unlikely to
   * be true for arbitrary fds. this is a workaround for a fundamental
   * flaw in the jobserver protocol. the GNU make maintainers are
   * aware of this but have not solved it yet.
   */
  struct stat statbuf;
  res = fstat(rfd, &statbuf);
  if (res != 0 || !S_ISFIFO(statbuf.st_mode))
    return false;
  res = fstat(wfd, &statbuf);
  if (res != 0 || !S_ISFIFO(statbuf.st_mode))
    return false;

  /*
   * next issue is that the read file descriptor for the jobserver may
   * or may not be non-blocking, and we're not allowed to set it to
   * the behavior that want (blocking) because the non-blocking flag
   * is part of the file instance (shared by many processes) and not a
   * property of our private file descriptor.
   *
   * if the read fd is non-blocking, we have two workarounds:
   *
   * - if /proc/self/fd/N is available (on Linux), open it in the
   *   default, blocking mode -- this works because we get a fresh
   *   file instance
   *
   * - otherwise, remember that the fd is non-blocking and backoff to
   *   a 100 Hz polling loop, which should be often enough to get good
   *   CPU utilization and seldom enough to not generate a huge amount
   *   of overhead. this should only happen on OS X.
   */
  int flags = fcntl(rfd, F_GETFL, 0);
  if (flags == -1)
    return false;
  if (flags & O_NONBLOCK) {
    char fn[256];
    sprintf(fn, "/proc/self/fd/%d", rfd);
    int newfd = open(fn, O_RDONLY);
    if (newfd == -1)
      nonblocking = true;
    else
      rfd = newfd;
  }

  parent_pid = getpid();
  read_fd = rfd;
  write_fd = wfd;
  max_subprocesses = _max_subprocesses;
  return true;
}

void jobServer::getToken() {
  if (nonblocking) {
    const struct timespec delay = {0, 10 * 1000 * 1000};
    while (read(read_fd, &token, 1) != 1)
      nanosleep(&delay, 0);
  } else {
    ENSURE(read(read_fd, &token, 1) == 1);
  }
}

void jobServer::putToken() {
  ENSURE(write(write_fd, &token, 1) == 1);
}

pid_t jobServer::limitedFork() {
  assert(getpid() == parent_pid);
  assert(read_fd != -1 && write_fd != -1);
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
  pid_t res = fork();
  if (res == -1)
    return -1;

  // parent returns immediately
  if (res != 0) {
    ++subprocesses;
    return res;
  }

  // child has to wait for a jobserver token
  getToken();

  return 0;
}

void jobServer::finishChild() {
  assert(getpid() != parent_pid);
  putToken();
  exit(0);
}

void jobServer::waitForAllChildren() {
  assert(getpid() == parent_pid);
  /*
   * every process forked by GNU make implicitly holds a single
   * jobserver token. here we temporarily return this token into the
   * pool while we're blocked waiting for children to finish. this is
   * a bit of a violation of the jobserver protocol, but it helps
   * prevent deadlock.
   *
   * to see the deadlock, consider a "make -j4". four clang processes
   * are spawned by make, and they spawn some number of sub-processes
   * for translation validation, all of which end up blocked waiting
   * for jobserver tokens -- because there are none. then, all four
   * clang processes end up here waiting for their children to finish,
   * and we're stuck forever.
   *
   * giving this token back doesn't, strictly speaking, prevent
   * deadlock, because it could be another clang that gets the token
   * instead of a TV subprocess -- but this seems unlikely assuming
   * the readers are queued in FIFO order.
   */
  putToken();
  int status;
  while (wait(&status) != -1) {
    if (WIFEXITED(status))
      --subprocesses;
  }
  getToken();
  if (subprocesses != 0) {
    std::cerr << "Alive2: Expected 0 subprocesses but have " << subprocesses
              << "\n";
  }
}
