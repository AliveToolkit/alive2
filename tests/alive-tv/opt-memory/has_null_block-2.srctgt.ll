; TEST-ARGS: -dbg

define i8* @src() {
  %q = call i8* @malloc(i32 1)
  ret i8* %q
}

define i8* @tgt() {
  %q = call i8* @malloc(i32 1)
  ret i8* %q
}

declare i8* @malloc(i32)

; CHECK: has_null_block: 1
