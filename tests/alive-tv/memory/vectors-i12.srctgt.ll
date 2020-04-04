; TEST-ARGS: -bidirectional
target datalayout="e"

define i24 @src(<2 x i12>* %p) {
  store <2 x i12> <i12 4094, i12 2050>, <2 x i12>* %p
  %p2 = bitcast <2 x i12>* %p to i24*
  %v = load i24, i24* %p2
  ret i24 %v
}

define i24 @tgt(<2 x i12>* %p) {
  store <2 x i12> <i12 4094, i12 2050>, <2 x i12>* %p
  ret i24 8400894
}

