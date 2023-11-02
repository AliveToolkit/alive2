define i17 @src(ptr %p) {
  store <2 x i17> <i17 1, i17 2>, ptr %p
  %load = load i17, ptr %p
  ret i17 %load
}

define i17 @tgt(ptr %p) {
  store <2 x i17> <i17 1, i17 2>, ptr %p
  ret i17 1
}
