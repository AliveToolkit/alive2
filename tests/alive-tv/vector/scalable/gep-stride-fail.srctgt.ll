; TEST-ARGS: --single-vscale=2
; ERROR: Target is more poisonous than source

define <vscale x 2 x i32> @src(ptr align 4 dereferenceable(32) %p) {
  %q = getelementptr <vscale x 2 x i32>, ptr %p, i64 1
  %r = load <vscale x 2 x i32>, ptr %q, align 4
  ret <vscale x 2 x i32> %r
}

define <vscale x 2 x i32> @tgt(ptr align 4 dereferenceable(32) %p) {
  %q = getelementptr i8, ptr %p, i64 8
  %r = load <vscale x 2 x i32>, ptr %q, align 4
  ret <vscale x 2 x i32> %r
}
