// TEST-ARGS: -O3 -mllvm -tv-quiet

int f(int x) {
  return x + 1 - x;
}

// CHECK-NOT: @f
