// TEST-ARGS: -mllvm -tv-elapsed-time

int f(int x) {
  return x + 1 - 1;
}

// CHECK: f: 
