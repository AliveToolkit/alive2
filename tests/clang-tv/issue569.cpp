// TEST-ARGS: -O3 -mllvm -tv-tgt-unroll=2 -mllvm -tv-src-unroll=2
void a() {
  for (;;)
    for (int b; b;)
      ;
}
