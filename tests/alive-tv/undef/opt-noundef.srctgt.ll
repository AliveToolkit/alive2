; TEST-ARGS: -se-verbose
declare void @f(i32 noundef)

define i32 @src(i32 %x) {
  call void @f(i32 %x)
  ret i32 %x
}

define i32 @tgt(i32 %x) {
  call void @f(i32 %x)
  ret i32 %x
}

; CHECK: return = %x / true
