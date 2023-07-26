declare void @f() noreturn
declare void @g()

define i8 @src(ptr %p) {
  call void @f()
  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @tgt(ptr %p) {
  %v = load i8, ptr %p
  call void @f()
  ret i8 %v
}

; ERROR: Source is more defined than target
