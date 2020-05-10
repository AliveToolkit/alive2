; TEST-ARGS: -dbg

define i8* @src() {
  ret i8* null
}

define i8* @tgt() {
  ret i8* null
}

; CHECK: has_null_block: 1
