// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/file.h"
#include <cstring>
#include <fstream>
#include <random>

using namespace std;

namespace util {

file_reader::file_reader(const char *filename, unsigned padding) {
  ifstream f(filename, ios::binary);
  if (!f)
    throw FileIOException();

  f.seekg(0, ios::end);
  sz = f.tellg();
  f.seekg(0, ios::beg);
  buf = new char[sz + padding];
  f.read(buf, sz);
  memset(buf + sz, 0, padding);
}

file_reader::~file_reader() {
  delete[] buf;
}

fs::path
makeUniqueFilePath(const std::string &dirname, const fs::path &fname,
                   bool always_add_suffix) {
  fs::path path = fs::path(dirname) / fname;
  if (!always_add_suffix && !fs::exists(path))
    return path;

  static default_random_engine re;
  static uniform_int_distribution<unsigned> rand;
  static bool seeded = false;

  if (!seeded) {
    random_device rd;
    re.seed(rd());
    seeded = true;
  }

  do {
    auto newname = fname.stem();
    newname += "_" + to_string(rand(re)) + ".txt";
    path.replace_filename(newname);
  } while (fs::exists(path));

  return path;
}


}
