; TEST-ARGS:

define i64 @src(i64 %x) {
  %A = shl nuw i64 %x, 3
  %B = lshr i64 %A, 1
  ret i64 %B
}