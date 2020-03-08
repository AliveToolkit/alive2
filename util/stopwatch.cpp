#include "util/stopwatch.h"
#include <cassert>
#include <iomanip>

using namespace std;
using namespace std::chrono;

static auto now() {
  return steady_clock::now();
}

namespace util {

StopWatch::StopWatch() {
  start = now();
}

void StopWatch::stop() {
  assert(!stopped);
  end = now();
  assert((stopped = true));
}

float StopWatch::seconds() const {
  assert(stopped);
  return duration_cast<duration<float>>(end - start).count();
}

ostream& operator<<(ostream &os, const StopWatch &w) {
  os << fixed << setprecision(2);
  auto seconds = w.seconds();
  if (seconds < 1)
    return os << (seconds / 1000.0) << " ms";
  return os << seconds << " s";
}

}
