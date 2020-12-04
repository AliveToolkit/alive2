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
#include <vector>

// TODO: read this carefully and make sure it's safe when pids are
// reused

std::tuple<pid_t, std::ostream *, int> parallel::doFork() {
  ensureParent();
  std::fflush(nullptr);
  readFromChildren();
  int index = children.size();
  children.emplace_back();
  childProcess &newKid = children.back();

  // reap zombies
  int status;
  while (waitpid((pid_t)(-1), &status, WNOHANG) > 0) {
    if (WIFEXITED(status))
      --subprocesses;
  }

  // if there are too many children already, wait for some to finish
  while (subprocesses >= max_subprocesses) {
    pid_t pid = wait(&status);
    if (pid != -1 && WIFEXITED(status))
      --subprocesses;
  }

  // this is how the child will send results back to the parent
  int res = pipe(newKid.pipe);
  if (res == -1)
    return std::make_tuple(-1, nullptr, -1);

  pid_t pid = fork();
  if (pid == (pid_t)-1)
    return std::make_tuple(-1, nullptr, -1);

  if (pid == 0) {
    /*
     * child -- close the read sides of all pipes including the new one
     */
    for (auto &c : children)
      ENSURE(close(c.pipe[0]) == 0);
  } else {
    /*
     * parent -- close the write side of the new pipe and mark the
     * read side as non-blocking
     */
    ENSURE(close(newKid.pipe[1]) == 0);
    int flags = fcntl(newKid.pipe[0], F_GETFL, 0);
    assert(flags != -1);
    res = fcntl(newKid.pipe[0], F_SETFL, flags | O_NONBLOCK);
    ENSURE(res != -1);
    ++subprocesses;
    newKid.pid = pid;
  }
  return std::make_tuple(pid, &newKid.output, index);
}

/*
 * return true only if all children have returned EOF
 */
bool parallel::readFromChildren() {
  const int maxRead = 4096;
  static char data[maxRead];
  bool allEOF = true;
  for (auto &c : children) {
    if (c.eof)
      continue;
    allEOF = false;
  again:
    size_t size = read(c.pipe[0], data, maxRead);
    if (size == (size_t)-1) {
      assert(errno == EAGAIN);
      continue;
    }
    if (size == 0) {
      c.eof = true;
      continue;
    }
    std::streambuf *pbuf = c.output.rdbuf();
    std::streamsize res = pbuf->sputn(data, size);
    ENSURE((size_t)res == size);
    /*
     * keep reading from this pipe until there's nothing left -- we
     * want to minimize the time TV processes spend blocked
     */
    goto again;
  }
  return allEOF;
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

void parallel::writeToParent() {
  ensureChild();
  childProcess &me = children.back();
  std::streambuf *pbuf = me.output.rdbuf();
  std::streamsize size = pbuf->pubseekoff(0, me.output.end);
  pbuf->pubseekoff(0, me.output.beg);
  char *data = new char[size];
  pbuf->sgetn(data, size);
  ENSURE(safe_write(me.pipe[1], data, size) == size);
  delete[] data;
}

void parallel::ensureParent() {
  assert(parent_pid != -1 && getpid() == parent_pid);
}

void parallel::ensureChild() {
  assert(parent_pid != -1 && getpid() != parent_pid);
}

bool parallel::init(int _max_subprocesses) {
  assert(parent_pid == -1);
  parent_pid = getpid();
  max_subprocesses = _max_subprocesses;
  return true;
}

void parallel::waitForAllChildren() {
  ensureParent();
  // FIXME: we could use poll() instead of this polling loop
  const struct timespec delay = {0, 100 * 1000 * 1000}; // 100ms
  while (!readFromChildren())
    nanosleep(&delay, 0);
  int status;
  while (wait(&status) != -1) {
    if (WIFEXITED(status))
      --subprocesses;
  }
  assert(subprocesses == 0);
}

void parallel::emitOutput(std::stringstream &parent_ss,
                          std::ofstream &out_file) {
  ensureParent();
  std::string line;
  std::regex rgx("^include\\(([0-9]+)\\)$");
  while (getline(parent_ss, line)) {
    std::smatch sm;
    if (std::regex_match(line, sm, rgx)) {
      assert(sm.size() == 2);
      int index = std::stoi(*std::next(sm.begin()));
      out_file << children[index].output.str();
    } else {
      assert(line.rfind("include(", 0) == std::string::npos);
      out_file << line << "\n";
    }
  }
}
