; CHECK: 0 correct

define zeroext i8 @f(i8, i8) {
  %x = sub i8 %0, %1
  ret i8 %x
}
