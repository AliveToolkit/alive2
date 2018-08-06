// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/ctx.h"
#include "smt/smt.h"
#include <z3.h>

namespace smt {

context ctx;

void context::init() {
  auto config = Z3_mk_config();
  Z3_set_param_value(config, "timeout", get_query_timeout());
  Z3_global_param_set("smt.ematching", "false");
  ctx = Z3_mk_context_rc(config);
  Z3_del_config(config);
}

void context::destroy() {
  Z3_del_context(ctx);
}

}
