target datalayout="E"
target triple = "powerpc64-unknown-linux-gnu"

define <4 x i2> @src(<4 x i2> %t){
  %v = bitcast <4 x i2> %t to i8
  %t2 = bitcast i8 %v to <4 x i2>
  ret <4 x i2> %t2
}

define <4 x i2> @tgt(<4 x i2> %t){
  ret <4 x i2> %t
}
