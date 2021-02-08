; TEST-ARGS: -dbg

define i8* @f() {
  ret i8* null
}

; CHECK: has_null_block: 1
