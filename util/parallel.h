#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <sys/types.h>

class parallel {
public:
  virtual ~parallel() {};
  
  /*
   * must be called before any other methods are used, and this object
   * must not be subsequently used if init() returns false
   */
  virtual bool init(int max_subprocesses) = 0;

  /*
   * called from parent; like fork(), returns non-zero to parent and
   * zero to child; does not fork until max_processes is respected
   * and, additionally, may throttle the child using e.g. the POSIX
   * jobserver
   */
  virtual pid_t limitedFork() = 0;

  /*
   * called from a child that has finished executing
   */
  [[noreturn]] virtual void finishChild() = 0;

  /*
   * called from parent, returns when all child processes have
   * terminated
   */
  virtual void waitForAllChildren() = 0;
};

class jobServer final : public parallel {
  char token;
  int read_fd = -1, write_fd = -1;
  int subprocesses = 0;
  int max_subprocesses;
  bool nonblocking = false;
  pid_t parent_pid = -1;
  void getToken();
  void putToken();
public:
  bool init(int max_subprocesses) override;
  pid_t limitedFork() override;
  void finishChild() override;
  void waitForAllChildren() override;
};

class unrestricted final : public parallel {
  int subprocesses = 0;
  pid_t parent_pid = -1;
  int max_subprocesses;
public:
  bool init(int max_subprocesses) override;
  pid_t limitedFork() override;
  void finishChild() override;
  void waitForAllChildren() override;
};
