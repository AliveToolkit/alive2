; TEST-ARGS: --single-vscale=2
; CHECK: Transformation seems to be correct!

define ptr @src(ptr %p) {
  %q = getelementptr <vscale x 2 x i32>, ptr %p, i64 1, i64 1
  ret ptr %q
}

define ptr @tgt(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 20
  ret ptr %q
}
