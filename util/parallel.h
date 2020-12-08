#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>
#include <sstream>
#include <sys/types.h>
#include <tuple>
#include <vector>

struct childProcess {
  int pipe[2];
  pid_t pid;
  /*
   * in a child process, this buffers its output until it is ready to
   * exit. for the parent process, this child's output is stored in
   * this buffer until we're ready to finally dump it when all
   * children have finished.
   */
  std::stringstream output;
  bool eof = false;
};

class parallel {
  pid_t parent_pid = -1;
  int max_subprocesses;
  void ensureParent();
  void ensureChild();
  int subprocesses = 0;
  /*
   * TODO JDR: this may work better as an unordered_map, I'll
   * investigate this later on
   */
  std::vector<childProcess> children;

protected:
  void writeToParent();
  bool readFromChildren();
  std::tuple<pid_t, std::ostream *, int> doFork();

public:
  virtual ~parallel() {}

  /*
   * must be called before any other methods are used, and this object
   * must not be subsequently used if init() returns false
   */
  virtual bool init(int _max_subprocesses);

  /*
   * called from parent; like fork(), returns non-zero to parent and
   * zero to child. it may also return -1 in which case there is no
   * child process and the other two returned values are meaningless.
   * this does not fork until max_processes is respected and,
   * additionally, may throttle the child using e.g. the POSIX
   * jobserver. the returned ostream is for the child to write its
   * results into and the integer is a unique identifier for this
   * child process.
   */
  virtual std::tuple<pid_t, std::ostream *, int> limitedFork() = 0;

  /*
   * called from a child that has finished executing
   */
  virtual void finishChild() = 0;

  /*
   * called from parent, returns when all child processes have
   * terminated
   */
  virtual void waitForAllChildren();

  void emitOutput(std::stringstream &parent_ss, std::ofstream &out_file);
};

class jobServer final : public parallel {
  char token;
  int read_fd = -1, write_fd = -1;
  bool nonblocking = false;
  void getToken();
  void putToken();

public:
  bool init(int max_subprocesses) override;
  std::tuple<pid_t, std::ostream *, int> limitedFork() override;
  void finishChild() override;
  void waitForAllChildren() override;
};

class unrestricted final : public parallel {
public:
  bool init(int max_subprocesses) override;
  std::tuple<pid_t, std::ostream *, int> limitedFork() override;
  void finishChild() override;
  void waitForAllChildren() override;
};
