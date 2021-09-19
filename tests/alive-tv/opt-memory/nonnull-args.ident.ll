; TEST-ARGS: -dbg
; CHECK: num_nonlocals: 1

define i8 @f(i8* nonnull %p) {
  %v = load i8, i8* %p
  ret i8 %v
}
