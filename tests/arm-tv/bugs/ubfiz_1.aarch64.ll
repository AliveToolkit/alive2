; TEST-ARGS:

define i32 @src(i32 %x) {
  %res = shl nuw i32 %x, 2
  ret i32 %res
}