define void @src(ptr %0) {
  %2 = load ptr, ptr %0, align 8
  %3 = getelementptr inbounds i64, ptr %2, i32 1
  %4 = getelementptr inbounds i64, ptr %3, i64 0
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %4, ptr null, i64 0, i1 false)
  ret void
}

define void @tgt(ptr %0) {
  %2 = load ptr, ptr %0, align 8
  %3 = getelementptr inbounds i64, ptr %2, i32 1
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %3, ptr null, i64 0, i1 false)
  ret void
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
