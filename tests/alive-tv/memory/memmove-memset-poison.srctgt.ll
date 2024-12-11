define void @src(ptr %src, ptr %dst) {
  %load.src = load i32, ptr %src
  store i32 poison, ptr %dst
  %gep.dst.1 = getelementptr i32, ptr %dst, i64 1
  store i32 %load.src, ptr %gep.dst.1
  ret void
}

define void @tgt(ptr %src, ptr %dst) {
  %gep.dst.1 = getelementptr i32, ptr %dst, i64 1
  call void @llvm.memmove.p0.p0.i64(ptr %gep.dst.1, ptr %src, i64 4, i1 false)
  call void @llvm.memset.p0.i64(ptr %dst, i8 poison, i64 4, i1 false)
  ret void
}
