target datalayout="e"

define i8 @f(<3 x i2>* %p) {
  %p2 = bitcast <3 x i2>* %p to i8*
  store i8 57, i8* %p2
  ret i8 57
}

define i8 @f2(<3 x i2>* %p) {
  %p2 = bitcast <3 x i2>* %p to i8*
  store i8 57, i8* %p2
  ret i8 57
}
