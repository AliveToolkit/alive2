; TEST-ARGS: -disable-undef-input

; Neither the mixed struct nor the containing array/struct has a memory layout.
%mixed = type { <vscale x 2 x i8>, i32 }
%nested = type { [2 x %mixed] }

define <vscale x 2 x i8> @src(<vscale x 2 x i8> %v, i32 %n) {
  %a = insertvalue %mixed poison, <vscale x 2 x i8> %v, 0
  %b = insertvalue %nested poison, %mixed %a, 0, 1
  %c = insertvalue %nested %b, i32 %n, 0, 1, 1
  %d = extractvalue %nested %c, 0, 1
  %r = extractvalue %mixed %d, 0
  ret <vscale x 2 x i8> %r
}

define <vscale x 2 x i8> @tgt(<vscale x 2 x i8> %v, i32 %n) {
  ret <vscale x 2 x i8> %v
}
