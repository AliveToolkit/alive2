// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/parallel.h"
#include "util/compiler.h"
#include <cassert>
#include <fcntl.h>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

void parallel::ensureParent() {
  assert(parent_pid != -1 && getpid() == parent_pid);
}

void parallel::ensureChild() {
  assert(parent_pid != -1 && getpid() != parent_pid);
}

bool parallel::init() {
  assert(parent_pid == -1);
  for (int i = 0; i < max_active_children; ++i) {
    pfd_map.push_back(-1);
    auto &p = pfd.emplace_back();
    p.fd = -1;
    p.events = POLL_IN;
  }
  parent_pid = getpid();
  return true;
}

void parallel::reapZombies() {
  while (waitpid((pid_t)-1, nullptr, WNOHANG) > 0)
    ;
}

std::tuple<pid_t, std::ostream *, int> parallel::limitedFork() {
  ensureParent();

  /*
   * we don't want child processes getting blocked writing to their
   * output pipes, so drain them all without blocking the parent
   */
  while (readFromChildren(/*blocking=*/false))
    reapZombies();

  /*
   * however, we'll need to block while there are too many outstanding
   * child processes
   */
  while (active_children >= max_active_children) {
    readFromChildren(/*blocking=*/true);
    reapZombies();
  }

  getToken();

  int index = children.size();
  children.emplace_back();
  childProcess &newKid = children.back();

  /*
   * amortize cost of copying part of the output stringstream to a new
   * one by not doing this all that often
   */
  if (index % 100 == 0)
    emitOutput();

  // this is how the child will send results back to the parent
  if (pipe(newKid.pipe) < 0)
    return {-1, nullptr, -1};

  std::fflush(nullptr);
  pid_t pid = fork();
  if (pid == (pid_t)-1)
    return {-1, nullptr, -1};

  if (pid == 0) {
    /*
     * child -- we inherited the read ends of potentially many pipes;
     * close all of the open ones (including the new one)
     */
    for (auto &c : children)
      if (!c.eof)
        ENSURE(close(c.pipe[0]) == 0);
    fd_to_parent = newKid.pipe[1];
  } else {
    /*
     * parent -- close the write side of the new pipe
     */
    ENSURE(close(newKid.pipe[1]) == 0);
    ++active_children;
    newKid.pid = pid;

    bool found = false;
    for (int i = 0; i < max_active_children; ++i) {
      if (pfd[i].fd == -1) {
        pfd[i].fd = newKid.pipe[0];
        pfd_map.at(i) = index;
        found = true;
        break;
      }
    }
    assert(found);
  }
  return {pid, &newKid.output, index};
}

/*
 * return true iff we got a state change from a child process (either
 * data or an EOF); if blocking, don't return until there is a state
 * change
 *
 * if !blocking, return false immediately if no children have changed
 * state
 *
 * if blocking, only return false when all children have returned EOF
 */
bool parallel::readFromChildren(bool blocking) {
  const int maxRead = 16 * 4096;
  static char data[maxRead];
  if (active_children == 0)
    return false;
  int res = poll(pfd.data(), max_active_children, blocking ? -1 : 0);
  if (res == -1) {
    perror("poll");
    exit(-1);
  }
  if (res == 0) {
    assert(!blocking);
    return false;
  }
  for (int i = 0; i < max_active_children; ++i) {
    if (pfd[i].revents == 0)
      continue;
    childProcess &c = children[pfd_map.at(i)];
    size_t size = read(c.pipe[0], data, maxRead);
    assert(size != (size_t)-1);
    if (size == 0) {
      c.eof = true;
      ENSURE(close(c.pipe[0]) == 0);
      --active_children;
      pfd[i].fd = -1;
    } else {
      c.output.write(data, size);
    }
  }
  return true;
}

/*
 * wrapper for write() that correctly handles short writes
 */
static ssize_t safe_write(int fd, const void *void_buf, size_t count) {
  const char *buf = (const char *)void_buf;
  ssize_t written = 0;
  while (count > 0) {
    ssize_t ret = write(fd, buf, count);
    // let caller deal with EOF and error conditions
    if (ret <= 0)
      return ret;
    count -= ret;
    buf += ret;
    written += ret;
  }
  return written;
}

/*
 * if is_timeout is true, we in signal handling context and can only
 * call async-safe functions
 */
void parallel::finishChild(bool is_timeout) {
  ensureChild();
  if (is_timeout) {
    const char *msg = "ERROR: Timeout asynchronous\n\n";
    safe_write(fd_to_parent, msg, strlen(msg));
  } else {
    childProcess &me = children.back();
    auto data = move(me.output).str();
    auto size = data.size();
    ENSURE(safe_write(me.pipe[1], data.c_str(), size) == (ssize_t)size);
  }
  putToken();
}

void parallel::finishParent() {
  ensureParent();
  while (readFromChildren(/*blocking=*/true))
    reapZombies();
  assert(active_children == 0);
  while (wait(nullptr) != -1)
    ;
  ENSURE(emitOutput());
}

/*
 * return true iff end of output has been reached
 */
bool parallel::emitOutput() {
  ensureParent();
  std::string line;
  std::regex rgx("^include\\(([0-9]+)\\)$");
  while (getline(parent_ss, line)) {
    std::smatch sm;
    if (std::regex_match(line, sm, rgx)) {
      assert(sm.size() == 2);
      int index = std::stoi(*std::next(sm.begin()));
      if (children[index].eof) {
        out_file << move(children[index].output).str();
        stringstream().swap(children[index].output); // free the RAM
      } else {
        /*
         * here, for two reasons, we swap parent_ss with a fresh one
         * containing a copy of the unwritten data. first, we've
         * already grabbed one line too many from parent_ss, and we
         * don't have a good way to put it back. second, we want to
         * free the RAM associated with data we've already read out of
         * the stringstream.
         */
        stringstream new_ss;
        new_ss << line << '\n';
        auto cur = parent_ss.tellg();
        new_ss << move(parent_ss).str().substr(cur);
        parent_ss.swap(new_ss);
        return false;
      }
    } else {
      assert(line.rfind("include(", 0) == std::string::npos);
      out_file << line << '\n';
    }
  }
  /*
   * reset the EOF flag since this process is going to keep writing
   * into parent_ss
   */
  parent_ss.clear();
  return true;
}
