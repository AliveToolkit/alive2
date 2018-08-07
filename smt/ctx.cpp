// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/ctx.h"
#include "smt/smt.h"
#include <z3.h>

namespace smt {

context ctx;

void context::init() {
  Z3_global_param_set("smt.ematching", "false");
  Z3_global_param_set("timeout", get_query_timeout());
  ctx = Z3_mk_context_rc(nullptr);
}

void context::destroy() {
  Z3_del_context(ctx);
}

}
