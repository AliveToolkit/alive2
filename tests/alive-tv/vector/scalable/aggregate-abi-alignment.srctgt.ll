; TEST-ARGS: --single-vscale=2 -disable-undef-input

; At vscale=2, the struct stores eight bytes but has a sixteen-byte allocation
; size. The array must add the eight bytes of allocation padding exactly once.
target datalayout = "e-p:64:64-a:64"
%pair = type { <vscale x 2 x i8>, <vscale x 2 x i8> }

define <vscale x 2 x i8> @src_array(ptr %p) {
  %a = load [2 x %pair], ptr %p, align 1
  %r = extractvalue [2 x %pair] %a, 1, 0
  ret <vscale x 2 x i8> %r
}

define <vscale x 2 x i8> @tgt_array(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 16
  %r = load <vscale x 2 x i8>, ptr %q, align 1
  ret <vscale x 2 x i8> %r
}

; Put the store on the target side so poisoning the byte after the struct's
; store size fails refinement instead of making the source less defined.
define i8 @src_store() {
  ret i8 42
}

define i8 @tgt_store() {
  %p = alloca [16 x i8], align 8
  %q = getelementptr i8, ptr %p, i64 8
  store i8 42, ptr %q, align 1
  store %pair zeroinitializer, ptr %p, align 1
  %r = load i8, ptr %q, align 1
  ret i8 %r
}
