; TEST-ARGS: -bidirectional
target datalayout="e"

define i24 @src(ptr %p) {
  store <2 x i12> <i12 4094, i12 2050>, ptr %p
  %v = load i24, ptr %p
  ret i24 %v
}

define i24 @tgt(ptr %p) {
  store <2 x i12> <i12 4094, i12 2050>, ptr %p
  ret i24 8400894
}

