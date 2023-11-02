target datalayout="e"

define void @src(ptr %p) {
  store <3 x i2> <i2 1, i2 2, i2 3>, ptr %p
  ret void
}

define void @tgt(ptr %p) {
  store <3 x i2> <i2 1, i2 2, i2 2>, ptr %p
  ret void
}

; ERROR: Mismatch in memory
