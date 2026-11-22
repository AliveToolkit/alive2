; TEST-ARGS: --single-vscale=4 -disable-undef-input

; Each vector has eight bytes of data and a sixteen-byte allocation size.
target datalayout = "e-p:64:64-v16:32"
%pair = type { <vscale x 2 x i8>, <vscale x 2 x i8> }

define <vscale x 2 x i8> @src_load(ptr %p) {
  %a = load %pair, ptr %p, align 4
  %r = extractvalue %pair %a, 1
  ret <vscale x 2 x i8> %r
}

define <vscale x 2 x i8> @tgt_load(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 16
  %r = load <vscale x 2 x i8>, ptr %q, align 4
  ret <vscale x 2 x i8> %r
}
