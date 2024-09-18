define void @src(ptr %P) {
  %arrayidx = getelementptr inbounds i32, ptr %P, i64 1
  store i32 0, ptr %arrayidx, align 4
  %add.ptr = getelementptr inbounds i32, ptr %P, i64 2
  call void @llvm.memset.p0.i64(ptr %add.ptr, i8 0, i64 11, i1 false)
  ret void
}

define void @tgt(ptr %P) {
  %arrayidx = getelementptr inbounds i8, ptr %P, i64 4
  call void @llvm.memset.p0.i64(ptr align(4) %arrayidx, i8 0, i64 15, i1 false)
  ret void
}

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
