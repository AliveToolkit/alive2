; TEST-ARGS: --single-vscale=4

%triple = type { <vscale x 2 x i1>, <vscale x 2 x i1>, <vscale x 2 x i1> }

define <vscale x 2 x i1> @src_struct(ptr %p) {
  %a = load %triple, ptr %p, align 1
  %r = extractvalue %triple %a, 2
  ret <vscale x 2 x i1> %r
}

define <vscale x 2 x i1> @tgt_struct(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 8
  %r = load <vscale x 2 x i1>, ptr %q, align 1
  ret <vscale x 2 x i1> %r
}

define <vscale x 2 x i1> @src_array(ptr %p) {
  %a = load [3 x <vscale x 2 x i1>], ptr %p, align 1
  %r = extractvalue [3 x <vscale x 2 x i1>] %a, 2
  ret <vscale x 2 x i1> %r
}

define <vscale x 2 x i1> @tgt_array(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 8
  %r = load <vscale x 2 x i1>, ptr %q, align 1
  ret <vscale x 2 x i1> %r
}
