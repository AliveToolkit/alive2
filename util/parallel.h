#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <ostream>
#include <poll.h>
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
  int max_active_children;
  int active_children = 0;
  std::vector<pollfd> pfd;
  std::vector<int> pfd_map;
  std::vector<childProcess> children;
  std::stringstream &parent_ss;
  std::ostream &out_file;
  void ensureParent();
  void ensureChild();
  void reapZombies();
  bool emitOutput();
  bool readFromChildren(bool blocking);

public:
  parallel(int max_active_children, std::stringstream &parent_ss,
           std::ostream &out_file)
      : max_active_children(max_active_children), parent_ss(parent_ss),
        out_file(out_file) {}
  virtual ~parallel() {}

  /*
   * must be called before any other methods are used, and this object
   * must not be subsequently used if init() returns false
   */
  virtual bool init();

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
  virtual void finishChild(bool is_timeout) = 0;

  /*
   * called from parent, returns when all child processes have
   * terminated
   */
  virtual void finishParent() = 0;
};

class posix final : public parallel {
  char token;
  int read_fd = -1, write_fd = -1;
  bool nonblocking = false;
  void getToken();
  void putToken();

public:
  posix(int max_active_children, std::stringstream &parent_ss,
        std::ostream &out_file)
      : parallel(max_active_children, parent_ss, out_file) {}
  bool init() override;
  std::tuple<pid_t, std::ostream *, int> limitedFork() override;
  void finishChild(bool is_timeout) override;
  void finishParent() override;
};

class fifo final : public parallel {
  char token;
  int pipe_fd = -1;
  void getToken();
  void putToken();

public:
  fifo(int max_active_children, std::stringstream &parent_ss,
       std::ostream &out_file)
      : parallel(max_active_children, parent_ss, out_file) {}
  bool init() override;
  std::tuple<pid_t, std::ostream *, int> limitedFork() override;
  void finishChild(bool is_timeout) override;
  void finishParent() override;
};

class unrestricted final : public parallel {
public:
  unrestricted(int max_active_children, std::stringstream &parent_ss,
               std::ostream &out_file)
      : parallel(max_active_children, parent_ss, out_file) {}
  bool init() override;
  std::tuple<pid_t, std::ostream *, int> limitedFork() override;
  void finishChild(bool is_timeout) override;
  void finishParent() override;
};

class null final : public parallel {
public:
  null(int max_active_children, std::stringstream &parent_ss,
       std::ostream &out_file)
      : parallel(max_active_children, parent_ss, out_file) {}
  bool init() override;
  std::tuple<pid_t, std::ostream *, int> limitedFork() override;
  void finishChild(bool is_timeout) override;
  void finishParent() override;
};
