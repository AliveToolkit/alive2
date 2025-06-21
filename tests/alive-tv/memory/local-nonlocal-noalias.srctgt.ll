%struct.f = type { [1024 x i32] }

define void @src(ptr %0, ptr byval(%struct.f) %1) {
  call void @llvm.memset.p0.i64(ptr %0, i8 0, i64 4096, i1 false)
  ret void
}

define void @tgt(ptr %0, ptr byval(%struct.f) readnone %1) {
  call void @llvm.memset.p0.i64(ptr %0, i8 0, i64 4096, i1 false)
  ret void
}

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
