define i8 @src(i32 range(i32 0, 10) %x, i32 range(i32 0, 10) %y) {
  %cmp = call i8 @llvm.scmp.i32.i8(i32 %x, i32 %y)
  ret i8 %cmp
}

define i8 @tgt(i32 %x, i32 %y) {
  %cmp = call i8 @llvm.ucmp.i32.i8(i32 %x, i32 %y)
  ret i8 %cmp
}
