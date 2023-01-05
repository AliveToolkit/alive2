; TEST-ARGS: --asm-only
; CHECK-NOT: 1 correct
; CHECK: w0, w0, w1

define i32 @f(i32, i32) {
  %x = add i32 %0, %1
  ret i32 %x
}
