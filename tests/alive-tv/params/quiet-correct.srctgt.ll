; TEST-ARGS: -quiet

define i32 @src(i32 %x) {
  ret i32 %x
}

define i32 @tgt(i32 %x) {
  %y = add i32 %x, 0
  ret i32 %y
}

; CHECK: Transformation seems to be correct!
; CHECK-NOT: define i32 @src
