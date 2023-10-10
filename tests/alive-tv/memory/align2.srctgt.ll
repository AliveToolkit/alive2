; ERROR: Value mismatch

define i32 @src(ptr align(4) %p) {
  call void @g(ptr align(2) %p)
  %v = load i32, ptr %p, align 4
  ret i32 %v
}

define i32 @tgt(ptr align(4) %p) {
  call void @g(ptr align(2) %p)
  %v = load i32, ptr %p, align 4
  ret i32 0
}

declare void @g(ptr)
