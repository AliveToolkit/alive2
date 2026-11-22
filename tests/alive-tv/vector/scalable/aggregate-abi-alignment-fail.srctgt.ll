; TEST-ARGS: --single-vscale=2 -disable-undef-input
; ERROR: Value mismatch

target datalayout = "e-p:64:64-a:64"
%pair = type { <vscale x 2 x i8>, <vscale x 2 x i8> }

define <vscale x 2 x i8> @src(ptr %p) {
  %p16 = getelementptr i8, ptr %p, i64 16
  %p24 = getelementptr i8, ptr %p, i64 24
  store i32 0, ptr %p16, align 1
  store i32 -1, ptr %p24, align 1
  %a = load [2 x %pair], ptr %p, align 1
  %r = extractvalue [2 x %pair] %a, 1, 0
  ret <vscale x 2 x i8> %r
}

define <vscale x 2 x i8> @tgt(ptr %p) {
  %p16 = getelementptr i8, ptr %p, i64 16
  %p24 = getelementptr i8, ptr %p, i64 24
  store i32 0, ptr %p16, align 1
  store i32 -1, ptr %p24, align 1
  %r = load <vscale x 2 x i8>, ptr %p24, align 1
  ret <vscale x 2 x i8> %r
}
