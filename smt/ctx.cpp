// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/ctx.h"
#include "smt/smt.h"
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <z3.h>

using namespace std;

static void z3_error_handler(Z3_context ctx, Z3_error_code err) {
  string_view str = Z3_get_error_msg(ctx, err);

  // harmless timeout
  if (str == "canceled")
    return;

  cerr << "Severe Z3 error: " << str << " [code=" << err << "]\n";
  exit(-1);
}

namespace smt {

context ctx;

void context::init() {
  Z3_global_param_set("model.partial", "true");
  Z3_global_param_set("smt.ematching", "false");
  Z3_global_param_set("smt.mbqi.max_iterations", "1000000");
  Z3_global_param_set("timeout", get_query_timeout());
  Z3_global_param_set("memory_high_watermark", "2147483648"); // 2 GBs
  // Disable Z3's use of UFs for NaNs when converting FPs to BVs
  // They generate incorrect formulas when quantifiers are involved
  Z3_global_param_set("rewriter.hi_fp_unspecified", "true");
  ctx = Z3_mk_context_rc(nullptr);
  Z3_set_error_handler(ctx, z3_error_handler);
}

void context::destroy() {
  Z3_del_context(ctx);
}

}
