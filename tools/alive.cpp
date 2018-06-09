// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/smt.h"
#include "tools/alive_parser.h"
#include "util/config.h"
#include "util/file.h"
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
    " -v:\t\tverbose mode\n"
    " -smt-stats:\tshow SMT statistics\n"
    " -skip-smt:\tassume all SMT queries are UNSAT\n"
    " -h / --help:\tshow this help\n";
}


int main(int argc, char **argv) {
  bool verbose = false;
  bool show_smt_stats = false;

  int argc_i = 1;
  for (; argc_i < argc; ++argc_i) {
    if (argv[argc_i][0] != '-')
      break;

    string_view arg(argv[argc_i]);
    if (arg == "-v")
      verbose = true;
    else if (arg == "-smt-stats")
      show_smt_stats = true;
    else if (arg == "-skip-smt")
      config::skip_smt = true;
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

  TransformPrintOpts print_opts;
  print_opts.print_fn_header = false;

  unsigned num_errors = 0;

  for (; argc_i < argc; ++argc_i) {
    cout << "Processing " << argv[argc_i] << "..\n";
    try {
      file_reader f(argv[argc_i], PARSER_READ_AHEAD);
      for (auto &t : parse(*f)) {
        t.print(cout, print_opts);
        cout << '\n';

        auto types = t.getTypings();
        if (!types) {
          cerr << "Doesn't type check!\n";
          ++num_errors;
          continue;
        }

        unsigned i = 0;
        for (; types; ++types) {
          t.fixupTypes(types);
          if (auto errs = t.verify()) {
            cerr << errs;
            ++num_errors;
            break;
          }
          cout << "\rDone: " << ++i << flush;
        }
        cout << '\n';
      }
    } catch (const FileIOException &e) {
      cerr << "Couldn't read the file" << endl;
      return -2;
    } catch (const ParseException &e) {
      cerr << "Parse error in line: " << e.lineno << ": " << e.str << endl;
      return -3;
    }
  }

  if (show_smt_stats) {
    // TODO
  }

  return num_errors;
}
