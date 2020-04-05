target datalayout="e"

define void @src(<3 x i2>* %p) {
  store <3 x i2> <i2 1, i2 2, i2 3>, <3 x i2>* %p
  ret void
}

define void @tgt(<3 x i2>* %p) {
  store <3 x i2> <i2 1, i2 2, i2 2>, <3 x i2>* %p
  ret void
}

; ERROR: Mismatch in memory
