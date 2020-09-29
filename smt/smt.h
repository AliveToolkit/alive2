#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <string>

namespace smt {

struct smt_initializer {
  smt_initializer();
  ~smt_initializer();
  void reset();

private:
  void init();
  void destroy();
};


void set_query_timeout(std::string ms);
const char* get_query_timeout();

void set_memory_limit(uint64_t limit);
bool hit_memory_limit();
bool hit_half_memory_limit();

void start_logging();

}
