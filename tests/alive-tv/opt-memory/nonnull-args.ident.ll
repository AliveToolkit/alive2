; TEST-ARGS: -dbg
; CHECK: num_nonlocals: 1

define i8 @f(ptr nonnull %p) {
  %v = load i8, ptr %p
  ret i8 %v
}
