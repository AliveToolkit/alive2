define half @src(half noundef %a, half noundef %b, half noundef %c) {
  %ret = call half @llvm.fmuladd.f32(half %a, half %b, half %c)
  ret half %ret
}

define half @tgt(half noundef %a, half noundef %b, half noundef %c) {
  %ret = call half @llvm.fma.f32(half %a, half %b, half %c)
  ret half %ret
}

declare half @llvm.fmuladd.f32(half, half, half)
declare half @llvm.fma.f32(half, half, half)
