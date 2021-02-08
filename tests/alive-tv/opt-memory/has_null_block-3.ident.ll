; TEST-ARGS: -dbg

define i8* @f(i8* %p) {
  ret i8* %p
}

; CHECK: has_null_block: 1
