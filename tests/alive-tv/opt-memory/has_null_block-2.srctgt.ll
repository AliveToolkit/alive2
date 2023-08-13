; TEST-ARGS: -dbg

define ptr @src() {
  %q = call ptr @malloc(i32 1)
  ret ptr %q
}

define ptr @tgt() {
  %q = call ptr @malloc(i32 1)
  ret ptr %q
}

declare ptr @malloc(i32)

; CHECK: has_null_block: 1
