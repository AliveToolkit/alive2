declare void @f() noreturn
declare void @g()

define i8 @src(i8* %p) {
  call void @f()
  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt(i8* %p) {
  %v = load i8, i8* %p
  call void @f()
  ret i8 %v
}

; ERROR: Source is more defined than target
