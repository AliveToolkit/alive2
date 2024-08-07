// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/file.h"
#include "util/random.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

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
  buf = make_unique<char[]>(sz + padding);
  f.read(buf.get(), sz);
  memset(buf.get() + sz, 0, padding);
}


string get_random_filename(const string &dir, const char *extension, const char *prefix) {
  // there's a low probability of race here
  auto newname = [&]() {
    ostringstream name;
    if (prefix) {
      name << prefix << '_';
    }
    name << get_random_str(12) << '.' << extension;
    return std::move(name).str();
  };
  fs::path path = fs::path(dir) / newname();
  while (fs::exists(path)) {
    path.replace_filename(newname());
  }
  return path.string();
}

}
