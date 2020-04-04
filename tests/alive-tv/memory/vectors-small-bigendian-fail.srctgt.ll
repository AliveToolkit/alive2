target datalayout="E"
target triple = "powerpc64-unknown-linux-gnu"

define void @src(<3 x i2>* %p) {
  ; 00011011 = 27
  store <3 x i2> <i2 1, i2 2, i2 3>, <3 x i2>* %p
  ret void
}

define void @tgt(<3 x i2>* %p) {
  store <3 x i2> <i2 1, i2 2, i2 2>, <3 x i2>* %p
  ret void
}

; ERROR: Mismatch in memory
