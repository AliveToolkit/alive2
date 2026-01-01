; TEST-ARGS: -se-verbose
declare void @g(i32 noundef)

define i32 @f(i32 %x) {
  call void @g(i32 %x)
  ret i32 %x
}

; CHECK: return = %x / non-poison=true
