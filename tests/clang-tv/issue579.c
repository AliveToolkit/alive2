// TEST-ARGS: -O3 -mllvm -tv-tgt-unroll=2 -mllvm -tv-src-unroll=2 -mllvm -tv-smt-to=10000 -mllvm -tv-disable-undef-input -mllvm -tv-disable-poison-input
short *d;

e(int a, int b, int c) {
  for (int f = 0; f < b; f += c)
    a = d[c];
}
