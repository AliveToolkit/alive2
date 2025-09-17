define i32 @src() {
entry:
  %s = alloca i128, align 8
  %x = getelementptr inbounds nuw i8, ptr %s, i64 8
  store i32 0, ptr %x, align 8
  %0 = load ptr, ptr %s, align 8
  %call = tail call i32 @multiply(ptr %0, i32 0)
  store ptr %s, ptr %s, align 8
  %1 = load i32, ptr %x, align 8
  ret i32 %1
}

define i32 @tgt() {
  %s = alloca i128, align 8
  %call = tail call i32 @multiply(ptr undef, i32 0)
  ret i32 0
}

declare i32 @multiply(ptr, i32)
