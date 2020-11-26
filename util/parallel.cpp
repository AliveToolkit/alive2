// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>

static char jobserver_token;
static int jobserver_read_fd = -1, jobserver_write_fd = -1;
static int subprocesses = 0;
static int max_subprocesses;

namespace parallel {

bool init(int _max_subprocesses) {
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
  int flags = fcntl(rfd, F_GETFL, 0);
  if (flags == -1)
    return false;
  flags &= ~O_NONBLOCK;
  if (fcntl(rfd, F_SETFL, flags) == -1)
    return false;
  jobserver_read_fd = rfd;
  jobserver_write_fd = wfd;
  max_subprocesses = _max_subprocesses;
  return true;
}

static void getToken() {
  int res [[maybe_unused]] = read(jobserver_read_fd, &jobserver_token, 1);
  assert(res == 1);
}

static void putToken() {
  int res [[maybe_unused]] = write(jobserver_write_fd, &jobserver_token, 1);
  assert(res == 1);
}

int limitedFork() {
  assert(jobserver_read_fd != -1 && jobserver_write_fd != -1);
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

void finishChild() {
  putToken();
  exit(0);
}

void waitForAllChildren() {
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
  assert(subprocesses == 0);
  getToken();
}

} // namespace parallel
