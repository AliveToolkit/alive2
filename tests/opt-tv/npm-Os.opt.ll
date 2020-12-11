; TEST-ARGS: -passes=default<Os>

define i32 @f() {
  ret i32 1
}

; src and tgt are identical, but opt-alive should not validate anything
; CHECK-NOT: Transformation
