// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/alive_parser.h"
#include "util/config.h"
#include "util/file.h"
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;


static void show_help() {
  cerr <<
    "Usage: alive2 <options> <files.opt>\n"
    "Options:\n"
    " -root-only\t\tCheck the expression's root only\n"
    " -v\t\t\tVerbose mode\n"
    " -smt-stats\t\tShow SMT statistics\n"
    " -smt-to:x\t\tTimeout for SMT queries in ms\n"
    " -max-mem:x\t\tMax memory consumption in MB (aprox)\n"
    " -smt-verbose\t\tPrint all SMT queries\n"
    " -skip-smt\t\tSkip all SMT queries\n"
    " -disable-poison-input\tAssume input variables can never be poison\n"
    " -disable-undef-input\tAssume input variables can never be undef\n"
    " -h / --help\t\tShow this help\n";
}


int main(int argc, char **argv) {
  bool verbose = false;
  bool show_smt_stats = false;
  bool root_only = false;

  int argc_i = 1;
  for (; argc_i < argc; ++argc_i) {
    if (argv[argc_i][0] != '-')
      break;

    string_view arg(argv[argc_i]);
    if (arg == "-root-only")
      root_only = true;
    else if (arg == "-v")
      verbose = true;
    else if (arg == "-smt-stats")
      show_smt_stats = true;
    else if (arg.compare(0, 8, "-smt-to:") == 0 && arg.size() > 8)
      smt::set_query_timeout(arg.substr(8).data());
    else if (arg.compare(0, 9, "-max-mem:") == 0 && arg.size() > 9)
      smt::set_memory_limit(strtoul(arg.substr(9).data(), nullptr, 10) *
                            1024 * 1024);
    else if (arg == "-smt-verbose")
      smt::solver_print_queries(true);
    else if (arg == "-tactic-verbose")
      smt::solver_tactic_verbose(true);
    else if (arg == "-skip-smt")
      config::skip_smt = true;
    else if (arg == "-disable-undef-input")
      config::disable_undef_input = true;
    else if (arg == "-disable-poison-input")
      config::disable_poison_input = true;
    else if (arg == "-h" || arg == "--help") {
      show_help();
      return 0;
    } else {
      cerr << "Unknown argument: " << arg << "\n\n";
      show_help();
      return -1;
    }
  }

  if (argc_i >= argc) {
    show_help();
    return -1;
  }

  if (verbose) {
    config::symexec_print_each_value = true;
  }

  smt::smt_initializer smt_init;
  parser_initializer parser_init;

  TransformPrintOpts print_opts;
  print_opts.print_fn_header = false;

  unsigned num_errors = 0;

  for (; argc_i < argc; ++argc_i) {
    cout << "Processing " << argv[argc_i] << "..\n";
    try {
      for (auto &t : parse(*file_reader(argv[argc_i], PARSER_READ_AHEAD))) {
        smt_init.reset();

        if (root_only && (!t.src.hasReturn() || !t.tgt.hasReturn())) {
          cerr << "Return instruction required with -root-only.\n";
          ++num_errors;
          continue;
        }

        t.print(cout, print_opts);
        cout << '\n';

        TransformVerify tv(t, !root_only);
        auto types = tv.getTypings();
        if (!types) {
          cerr << "Doesn't type check!\n";
          ++num_errors;
          continue;
        }

        unsigned i = 0;
        bool correct = true;
        for (; types; ++types) {
          tv.fixupTypes(types);
          if (auto errs = tv.verify()) {
            cerr << errs;
            ++num_errors;
            correct = false;
            break;
          }
          cout << "\rDone: " << ++i << flush;
        }
        cout << '\n';
        if (correct)
          cout << "Optimization is correct!\n";
      }
    } catch (const FileIOException &e) {
      cerr << "Couldn't read the file" << endl;
      return -2;
    } catch (const ParseException &e) {
      cerr << "Parse error in line: " << e.lineno << ": " << e.str << endl;
      return -3;
    }
  }

  if (show_smt_stats)
    smt::solver_print_stats(cout);

  return num_errors;
}
