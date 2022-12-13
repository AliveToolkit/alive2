; TEST-ARGS: -passes=default<Os>

define i32 @f() {
  ret i32 1
}

; CHECK: Transformation seems
