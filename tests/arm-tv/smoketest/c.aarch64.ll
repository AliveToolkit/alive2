; TEST-ARGS:

define i32 @f(i32 %a) {
  %x = add i32 %a, 77
  ret i32 %x
}
