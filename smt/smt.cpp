// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/smt.h"
#include "smt/ctx.h"
#include "smt/solver.h"
#include <cstdint>
#include <z3.h>

namespace smt {

smt_initializer::smt_initializer() {
  ctx.init();
  solver_init();
}

smt_initializer::~smt_initializer() {
  solver_destroy();
  ctx.destroy();
  Z3_finalize_memory();
}


static const char *query_timeout = "10000";

void set_query_timeout(const char *ms) {
  query_timeout = ms;
}

const char* get_query_timeout() {
  return query_timeout;
}


// FIXME make this configurable
static uint64_t z3_memory_limit = 1ull << 30; // 1 GB

bool hit_memory_limit() {
  return Z3_get_estimated_alloc_size() >= z3_memory_limit;
}

bool hit_half_memory_limit() {
  return Z3_get_estimated_alloc_size() >= (z3_memory_limit / 2);
}

}
