target datalayout="E"
target triple = "powerpc64-unknown-linux-gnu"

define i8 @f(<3 x i2>* %p) {
  %p2 = bitcast <3 x i2>* %p to i8*
  store i8 27, i8* %p2
  ret i8 27
}

define i8 @f2(<3 x i2>* %p) {
  %p2 = bitcast <3 x i2>* %p to i8*
  store i8 27, i8* %p2
  ret i8 27
}
