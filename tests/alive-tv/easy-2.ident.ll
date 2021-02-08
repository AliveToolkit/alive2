define i32 @foo(i32, i32) {
  %x1 = add i32 %0, 1
  %x2 = mul i32 %x1, 10
  %x3 = add i32 %x2, 7
  %x4 = udiv i32 %x3, 5
  ret i32 %x4
}
