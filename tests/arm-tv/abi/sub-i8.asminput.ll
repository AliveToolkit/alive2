; CHECK: 1 correct

define i32 @f(i32, i32) {
  %x = sub i32 %0, %1
  ret i32 %x
}
