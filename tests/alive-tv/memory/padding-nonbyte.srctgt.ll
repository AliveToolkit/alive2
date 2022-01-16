define void @src(i28* %in) {
  %val2 = load i28, i28* %in, align 4
  store i28 %val2, i28* %in, align 4
  ret void
}

define void @tgt(i28* %in) {
  ret void
}
