declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

define void @src(i8* %p1) {
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p1)

  ret void
}

define void @tgt(i8* %p1) {
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p1)

  unreachable
}

; ERROR: Source is more defined than target
