define i32 @src(i32, i32) {
  %x = add nsw i32 %0, %1
  ret i32 %x
}

define i32 @tgt(i32, i32) {
  %x = add i32 %0, %1
  ret i32 %x
}
