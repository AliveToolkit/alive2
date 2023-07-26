declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define void @src() {
  %p = alloca i32

  %p1 = getelementptr i8, ptr %p, i32 1
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p1)

  ret void
}

define void @tgt() {
  %p = alloca i32

  %p1 = getelementptr i8, ptr %p, i32 1
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p1)

  unreachable
}

; ERROR: Source is more defined than target
