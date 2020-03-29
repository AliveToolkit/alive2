; TEST-ARGS: -succinct

define i32 @src(i32 %x) {
  ret i32 %x
}

define i32 @tgt(i32 %x) {
  %y = add i32 %x, 1
  ret i32 %y
}

; CHECK: Transformation doesn't verify!
; CHECK-NOT: define i32 @src
; CHECK-NOT: ERROR:
