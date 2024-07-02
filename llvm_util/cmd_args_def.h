// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#ifdef ARGS_SRC_TGT
config::src_unroll_cnt = opt_src_unrolling_factor;
config::tgt_unroll_cnt = opt_tgt_unrolling_factor;
#else
config::src_unroll_cnt = opt_unrolling_factor;
config::tgt_unroll_cnt = opt_unrolling_factor;
#endif
config::disable_undef_input = opt_disable_undef;
config::disable_poison_input = opt_disable_poison;
config::tgt_is_asm = opt_tgt_is_asm;
config::fail_if_src_is_ub = opt_fail_if_src_is_ub;
config::symexec_print_each_value = opt_se_verbose;
smt::set_query_timeout(to_string(opt_smt_to));
smt::set_memory_limit((uint64_t)opt_smt_max_mem * 1024 * 1024);
smt::set_random_seed(to_string(opt_smt_random_seed));
config::skip_smt = opt_smt_skip;
config::smt_benchmark_dir = opt_smt_bench_dir;
smt::solver_print_queries(opt_smt_verbose);
smt::solver_tactic_verbose(opt_tactic_verbose);
config::debug = opt_debug;
config::max_offset_bits = opt_max_offset_in_bits;
config::max_sizet_bits  = opt_max_sizet_in_bits;

if ((config::disallow_ub_exploitation = opt_disallow_ub_exploitation)) {
  config::disable_undef_input = true;
  config::disable_poison_input = true;
}

config::fp_encoding_mode = opt_uf_float ? config::FpEncodingMode::UninterpretedFunctions : config::FpEncodingMode::FloatingPoint;

func_names.insert(opt_funcs.begin(), opt_funcs.end());

if (!report_dir_created && !opt_report_dir.empty()) {
  try {
    fs::create_directories(opt_report_dir.getValue());
  } catch (...) {
    cerr << "Alive2: Couldn't create report directory!" << endl;
    exit(1);
  }
  auto &source_file = ARGS_MODULE_VAR->getSourceFileName();
  fs::path fname = source_file.empty() ? "alive.txt" : source_file;
  fname.replace_extension(".txt");
  fs::path path = fs::path(opt_report_dir.getValue()) / fname.filename();

  if (!opt_overwrite_reports) {
    do {
      auto newname = fname.stem();
      if (newname.compare("-") == 0 || newname.compare("<stdin>") == 0)
        newname = "in";
      newname += "_" + get_random_str(8) + ".txt";
      path.replace_filename(newname);
    } while (fs::exists(path));
  }

  out_file.open(path);
  out = &out_file;
  if (!out_file.is_open()) {
    cerr << "Alive2: Couldn't open report file!" << endl;
    exit(1);
  }

  report_filename = path;
  *out << "Source: " << source_file << endl;
  report_dir_created = true;

  if (opt_smt_log) {
    fs::path path_z3log = path;
    path_z3log.replace_extension("z3_log.txt");
    smt::start_logging(path_z3log.c_str());
  }
} else if (opt_report_dir.empty()) {
  out = &cout;
  if (opt_smt_log)
    smt::start_logging();
}

if (!opt_outputfile.empty()) {
  out_file.open(opt_outputfile);
  out = &out_file;
  if (!opt_report_dir.empty()) {
    cerr << "Cannot use -o and -report-dir at the same time!\n";
    exit(-1);
  }
}

util::config::set_debug(*out);


if (opt_cache) {
#ifdef NO_REDIS_SUPPORT
  cerr << "REDIS support not compiled in!\n";
  exit(1);
#else
  cache = make_unique<Cache>(opt_cache_port, opt_cache_allow_version_mismatch);
#endif
}
