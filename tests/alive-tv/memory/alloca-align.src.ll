define void @f1() {
  %p = alloca i32, align 1
  ret void
}

define void @f2() {
  %p = alloca i32, align 1
  %p0 = bitcast i32* %p to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)
  ret void
}

define void @f3() {
  %p = alloca i32, align 1
  ret void
}

define void @f4() {
  %p = alloca i32, align 1
  %p0 = bitcast i32* %p to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)
  ret void
}


declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)
