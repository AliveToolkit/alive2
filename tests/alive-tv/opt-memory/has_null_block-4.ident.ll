; TEST-ARGS: -dbg

@x = global ptr null

define ptr @f() {
  %q = load ptr, ptr @x
  ret ptr %q
}
; CHECK: has_null_block: 1
