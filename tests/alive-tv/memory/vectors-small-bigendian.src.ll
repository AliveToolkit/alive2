; ERROR: Mismatch in memory

target datalayout="E"

define i8 @f(ptr %p) {
  ; 00011011 = 27
  store <3 x i2> <i2 1, i2 2, i2 3>, ptr %p
  %q = load <3 x i2>, ptr %p
  %w = bitcast <3 x i2> %q to i6
  %v = zext i6 %w to i8
  ret i8 %v
}

define i8 @f2(ptr %p) {
  store <3 x i2> <i2 1, i2 2, i2 3>, ptr %p
  %v = load i8, ptr %p
  ret i8 %v
}

