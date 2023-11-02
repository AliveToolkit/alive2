define void @f1() {
  %p = alloca i32, align 1
  ret void
}

define void @f2() {
  %p = alloca i32, align 1
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)
  ret void
}

define void @f3() {
  %p = alloca i32, align 1
  ret void
}

define void @f4() {
  %p = alloca i32, align 1
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)
  ret void
}


declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)
