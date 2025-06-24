define { half, i32 } @src() {
  %frexp = call { half, i32 } @llvm.frexp.f16.i32(half 0.0)
  ret { half, i32 } %frexp
}

define { half, i32 } @tgt() {
  ret { half, i32 } zeroinitializer
}
