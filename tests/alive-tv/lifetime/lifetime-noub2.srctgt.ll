declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define void @src(ptr %p1) {
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p1)

  ret void
}

define void @tgt(ptr %p1) {
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p1)

  unreachable
}

; ERROR: Source is more defined than target
