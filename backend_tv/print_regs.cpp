#include <cstdio>

static unsigned toInt(bool b) {
  return b ? 1 : 0;
}

extern "C" {

void printRegs(bool N, bool Z, bool C, bool V) {
  unsigned flags =
    (toInt(N) << 3) |
    (toInt(Z) << 2) |
    (toInt(C) << 1) |
    (toInt(V) << 0);
  printf("N = %d, Z = %d, C = %d, V = %d; pstate = %x\n",
	 N, Z, C, V, flags);
}

}
