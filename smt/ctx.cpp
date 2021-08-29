// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "smt/ctx.h"
#include "smt/smt.h"
#include "util/config.h"
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <z3.h>

using namespace std;
using util::config::dbg;

static void z3_error_handler(Z3_context ctx, Z3_error_code err) {
  string_view str = Z3_get_error_msg(ctx, err);

  // harmless timeout
  if (str == "canceled")
    return;

  dbg() << "Severe Z3 error: " << str << " [code=" << err << "]\n";
  _Exit(-1);
}

namespace smt {

context ctx;

void context::init() {
  Z3_global_param_set("model.partial", "true");
  Z3_global_param_set("smt.ematching", "false");
  Z3_global_param_set("smt.mbqi.max_iterations", "1000000");
  Z3_global_param_set("smt.random_seed", get_random_seed());
  Z3_global_param_set("timeout", get_query_timeout());
  Z3_global_param_set("memory_high_watermark", "2147483648"); // 2 GBs
  // Disable Z3's use of UFs for NaNs when converting FPs to BVs
  // They generate incorrect formulas when quantifiers are involved
  Z3_global_param_set("rewriter.hi_fp_unspecified", "true");
  ctx = Z3_mk_context_rc(nullptr);
  Z3_set_error_handler(ctx, z3_error_handler);

  no_timeout_param = Z3_mk_params(ctx);
  Z3_params_inc_ref(ctx, no_timeout_param);
  Z3_params_set_uint(ctx, no_timeout_param,
                     Z3_mk_string_symbol(ctx, "timeout"), 0);
}

void context::destroy() {
  Z3_params_dec_ref(ctx, no_timeout_param);
  Z3_del_context(ctx);
}

}


// close log file on program exit
namespace {
struct close_log {
  ~close_log() {
    Z3_close_log();
  }
};

close_log close;
}
