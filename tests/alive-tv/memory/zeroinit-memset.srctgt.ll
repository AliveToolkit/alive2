; From Transforms/MemCpyOpt/fca2memcpy.ll
target datalayout = "e-i64:64-f80:128-n8:16:32:64"

%S = type { ptr, i32 }

define void @src(ptr %src, ptr %dst) {
  %1 = load %S, ptr %src
  store %S zeroinitializer, ptr %src, align 8
  store %S %1, ptr %dst
  ret void
}

define void @tgt(ptr %src, ptr %dst) {
  %1 = load %S, ptr %src
  call void @llvm.memset.p0i8.i64(ptr align 8 %src, i8 0, i64 16, i1 false)
  store %S %1, ptr %dst
  ret void
}

declare void @llvm.memset.p0i8.i64(ptr, i8, i64, i1)
