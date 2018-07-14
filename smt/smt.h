#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

namespace smt {

struct smt_initializer {
  smt_initializer();
  ~smt_initializer();
  void reset();

private:
  void init();
  void destroy();
};


void set_query_timeout(const char *ms);
const char* get_query_timeout();

bool hit_memory_limit();
bool hit_half_memory_limit();

}
