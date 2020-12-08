// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/parallel.h"
#include "util/compiler.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

bool jobServer::init(int max_subprocesses) {
  ENSURE(parallel::init(max_subprocesses));
  auto env = getenv("MAKEFLAGS");
  if (!env)
    return false;
  
  int res = 0;
  if (auto sub = strstr(env, "jobserver-auth=")) // new GNU make
    res = sscanf(sub, "jobserver-auth=%d,%d", &read_fd, &write_fd);
  else if (auto sub = strstr(env, "jobserver-fds=")) // old GNU make
    res = sscanf(sub, "jobserver-fds=%d,%d", &read_fd, &write_fd);
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
  res = fstat(read_fd, &statbuf);
  if (res != 0 || !S_ISFIFO(statbuf.st_mode))
    return false;
  res = fstat(write_fd, &statbuf);
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
  int flags = fcntl(read_fd, F_GETFL, 0);
  if (flags == -1)
    return false;
  if (flags & O_NONBLOCK) {
    string fn = "/proc/self/fd/" + to_string(read_fd);
    int newfd = open(fn.c_str(), O_RDONLY);
    if (newfd < 0)
      nonblocking = true;
    else
      read_fd = newfd;
  }
  return true;
}

void jobServer::getToken() {
  if (nonblocking) {
    const struct timespec delay = {0, 10 * 1000 * 1000}; // 10 ms
    while (read(read_fd, &token, 1) != 1)
      nanosleep(&delay, 0);
  } else {
    ENSURE(read(read_fd, &token, 1) == 1);
  }
}

void jobServer::putToken() {
  ENSURE(write(write_fd, &token, 1) == 1);
}

tuple<pid_t, ostream *, int> jobServer::limitedFork() {
  assert(read_fd != -1 && write_fd != -1);
  auto res = doFork();
  // child now waits for a jobserver token
  if (get<0>(res) == 0)
    getToken();
  return res;
}

void jobServer::finishChild() {
  writeToParent();
  putToken();
  exit(0);
}

void jobServer::waitForAllChildren() {
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
  parallel::waitForAllChildren();
  getToken();
}
