; TEST-ARGS: -se-verbose
define i32 @src(i32 %x) {
  %v = add i32 %x, 1
  %c = icmp eq i32 %x, 5
  br i1 %c, label %A, label %B
A:
  ret i32 %x
B:
  ret i32 %x
}
; CHECK: return = %x / true
