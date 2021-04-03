// TEST-ARGS: -mllvm -tv-time-verify

int f(int x) {
  return x + 1 - 1;
}

// CHECK: Took 
