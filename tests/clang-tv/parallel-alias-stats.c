// TEST-ARGS: -O3 -mllvm -tv-alias-stats -mllvm -tv-parallel=unrestricted -mllvm --max-subprocesses=2

int f(int *x, int *y) {
  return *x + *y;
}

// CHECK: Alias sets statistics
