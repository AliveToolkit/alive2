// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/file.h"
#include "util/random.h"
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace std;
namespace fs = std::filesystem;

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


string get_random_filename(const string &dir, const char *extension) {
  // there's a low probability of race here
  auto newname = [&]() { return get_random_str(12) + '.' + extension; };
  fs::path path = fs::path(dir) / newname();
  while (fs::exists(path)) {
    path.replace_filename(newname());
  }
  return path;
}

}
