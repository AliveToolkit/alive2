; TEST-ARGS: -dbg

define ptr @f() {
  ret ptr null
}

; CHECK: has_null_block: 1
