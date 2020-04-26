; TEST-ARGS: -dbg

define i8* @src(i8* %p) {
  ret i8* %p
}

define i8* @tgt(i8* %p) {
  ret i8* %p
}

; CHECK: has_null_block: 1
