// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/ctx.h"
#include <z3.h>

namespace smt {

context ctx;

void context::init() {
  auto config = Z3_mk_config();
  // TODO: make this customizable
  Z3_set_param_value(config, "timeout", "1000");
  ctx = Z3_mk_context_rc(config);
  Z3_del_config(config);
}

void context::destroy() {
  Z3_del_context(ctx);
}

}
