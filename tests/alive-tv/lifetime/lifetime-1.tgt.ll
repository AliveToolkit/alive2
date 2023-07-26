declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define void @f1() {
  %p = alloca i32
  %p0 = bitcast ptr %p to ptr
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p0)
  ret void
}

define i32 @f2() {
  %p = alloca i32
  %p0 = bitcast ptr %p to ptr
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p0)
  store i32 10, ptr %p
  %v = load i32, ptr %p
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p0)
  ret i32 %v
}

