define { half, i32 } @src() {
  %frexp = call { half, i32 } @llvm.frexp.f16.i32(half 0.0)
  ret { half, i32 } %frexp
}

define { half, i32 } @tgt() {
  ret { half, i32 } zeroinitializer
}

define <2 x half> @src2(<2 x half> %h) {
  %r = call { <2 x half>, <2 x i32> } @llvm.frexp.v2f32.v2i32(<2 x half> %h)
  %e0 = extractvalue { <2 x half>, <2 x i32> } %r, 0
  ret <2 x half> %e0
}

define <2 x half> @tgt2(<2 x half> %h) {
  %h.i0 = extractelement <2 x half> %h, i64 0
  %r.i0 = call { half, i32 } @llvm.frexp.f16.i32(half %h.i0)
  %h.i1 = extractelement <2 x half> %h, i64 1
  %r.i1 = call { half, i32 } @llvm.frexp.f16.i32(half %h.i1)
  %e0.elem0 = extractvalue { half, i32 } %r.i0, 0
  %e0.elem01 = extractvalue { half, i32 } %r.i1, 0
  %e0.upto0 = insertelement <2 x half> poison, half %e0.elem0, i64 0
  %e0 = insertelement <2 x half> %e0.upto0, half %e0.elem01, i64 1
  ret <2 x half> %e0
}
