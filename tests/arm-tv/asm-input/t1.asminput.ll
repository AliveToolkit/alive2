; CHECK: No such file or directory

define i32 @f(i32, i32) {
  %x = add i32 %0, %1
  ret i32 %x
}
