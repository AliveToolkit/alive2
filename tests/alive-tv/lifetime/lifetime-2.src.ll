declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define i32 @f_end() {
  %p = alloca i32

  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)

  store i32 10, ptr %p
  %v = load i32, ptr %p

  ret i32 %v
}

define i32 @f_start() {
  %p = alloca i32

  store i32 10, ptr %p
  ; load before lifetime.start can be optimized to undef
  %v = load i32, ptr %p

  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)

  ret i32 %v
}

