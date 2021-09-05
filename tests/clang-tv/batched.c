// TEST-ARGS: -O3 -mllvm -tv-batch-opts

int f(int x) {
  return x + 1 - 1;
}

// CHECK: -- FROM THE BITCODE AFTER
// CHECK: -- TO THE BITCODE AFTER
// CHECK: -- DONE: