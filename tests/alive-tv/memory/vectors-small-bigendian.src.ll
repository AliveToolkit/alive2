target datalayout="E"
target triple = "powerpc64-unknown-linux-gnu"

define i8 @f(<3 x i2>* %p) {
  ; 00011011 = 27
  store <3 x i2> <i2 1, i2 2, i2 3>, <3 x i2>* %p
  %q = load <3 x i2>, <3 x i2>* %p
  %w = bitcast <3 x i2> %q to i6
  %v = zext i6 %w to i8
  ret i8 %v
}

define i8 @f2(<3 x i2>* %p) {
  store <3 x i2> <i2 1, i2 2, i2 3>, <3 x i2>* %p
  %p2 = bitcast <3 x i2>* %p to i8*
  %v = load i8, i8* %p2
  ret i8 %v
}

