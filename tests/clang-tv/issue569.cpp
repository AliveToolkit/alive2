// TEST-ARGS: -O3 -mllvm -tv-tgt-unroll=2 -mllvm -tv-src-unroll=2
void a() {
  for (;;)
    for (int b; b;)
      ;
}

// Since it has 'ERROR: Precondition is always false', this CHECK is necessary
// (otherwise this test will fail)
// CHECK: Transformation seems to be correct
