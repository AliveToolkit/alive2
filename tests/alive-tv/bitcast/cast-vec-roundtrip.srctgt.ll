define i8 @src(i8 %v){
  %t = bitcast i8 %v to <4 x i2>
  %v2 = bitcast <4 x i2> %t to i8
  ret i8 %v2
}

define i8 @tgt(i8 %v) {
  ret i8 %v
}
