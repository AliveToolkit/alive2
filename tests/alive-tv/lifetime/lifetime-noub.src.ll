declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

define void @f() {
  %p = alloca i32

  %p0 = bitcast i32* %p to i8*
  %p1 = getelementptr i8, i8* %p0, i32 1
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p1)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p1)

  ret void
}

; ERROR: Source is more defined than target
