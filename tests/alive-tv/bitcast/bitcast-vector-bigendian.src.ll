target datalayout="E"
target triple = "powerpc64-unknown-linux-gnu"

define i4 @f1() {
  ; returns: 1101
  %v = bitcast <4 x i1> <i1 1, i1 1, i1 0, i1 1> to i4
  ret i4 %v
}

define i8 @f2() {
  ; returns: 00011011
  %v = bitcast <4 x i2> <i2 0, i2 1, i2 2, i2 3> to i8
  ret i8 %v
}

define i12 @f3() {
  ; returns: 00000000 01010011
  %v = bitcast <4 x i3> <i3 0, i3 1, i3 2, i3 3> to i12
  ret i12 %v
}

define i16 @f4() {
  ; returns: 00000001 00100011
  %v = bitcast <4 x i4> <i4 0, i4 1, i4 2, i4 3> to i16
  ret i16 %v
}

