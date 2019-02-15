// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/smt.h"
#include "smt/ctx.h"
#include "smt/solver.h"
#include <cstdint>
#include <z3.h>

using namespace std;

namespace smt {

smt_initializer::smt_initializer() {
  init();
}

void smt_initializer::reset() {
  destroy();
  Z3_reset_memory();
  init();
}

smt_initializer::~smt_initializer() {
  destroy();
  Z3_finalize_memory();
}

void smt_initializer::init() {
  ctx.initialize();
  solver_init();
}

void smt_initializer::destroy() {
  solver_destroy();
  ctx.destroy();
}


static string query_timeout = "10000";

void set_query_timeout(string ms) {
  query_timeout = move(ms);
}

const char* get_query_timeout() {
  return query_timeout.c_str();
}


static uint64_t z3_memory_limit = 1ull << 30; // 1 GB

void set_memory_limit(uint64_t limit) {
  z3_memory_limit = limit;
}

bool hit_memory_limit() {
  return Z3_get_estimated_alloc_size() >= z3_memory_limit;
}

bool hit_half_memory_limit() {
  return Z3_get_estimated_alloc_size() >= (z3_memory_limit / 2);
}

}
