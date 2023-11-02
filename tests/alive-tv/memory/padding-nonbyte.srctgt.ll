define void @src(ptr %in) {
  %val2 = load i28, ptr %in, align 4
  store i28 %val2, ptr %in, align 4
  ret void
}

define void @tgt(ptr %in) {
  ret void
}
