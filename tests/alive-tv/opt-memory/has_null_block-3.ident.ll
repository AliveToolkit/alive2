; TEST-ARGS: -dbg

define ptr @f(ptr %p) {
  ret ptr %p
}

; CHECK: has_null_block: 1
