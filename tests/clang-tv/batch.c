// TEST-ARGS: -O3 -mllvm -tv-batch-opts

int f(int x) {
  return x + 1 - x;
}

// CHECK: From
// CHECK: To
