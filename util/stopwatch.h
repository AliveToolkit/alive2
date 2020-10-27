#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <chrono>
#include <ostream>
#include <functional>

namespace util {

class StopWatch {
  std::chrono::steady_clock::time_point start, end;
#ifndef NDEBUG
  bool stopped = false;
#endif

public:
  StopWatch();
  void stop();
  float seconds() const;

  friend std::ostream& operator<<(std::ostream &os, const StopWatch &w);
};

class ScopedWatch {
  StopWatch sw;
  std::function<void(const StopWatch &)> callback;

public:
  ScopedWatch(std::function<void(const StopWatch &)> &&callback)
      : callback(move(callback)) {}
  ~ScopedWatch();
};

}
