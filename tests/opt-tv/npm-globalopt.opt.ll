; TEST-ARGS: -passes=globalopt

@x = constant i32 0
define i32 @f() {
  %v = load i32, i32* @x
  ret i32 %v
}

; src and tgt are identical, but opt-alive hould not validate anything
; CHECK-NOT: Transformation
