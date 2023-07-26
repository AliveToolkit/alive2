declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define i8 @gep1() {
  %p = alloca i32

  %p1 = getelementptr i8, ptr %p, i32 1
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  store i8 10, ptr %p1
  %v = load i8, ptr %p1
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)

  ret i8 %v
}

define i8 @gep2() {
  %p = alloca i32

  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  %p1 = getelementptr i8, ptr %p, i32 1
  store i8 10, ptr %p1
  %v = load i8, ptr %p1
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)

  ret i8 %v
}

define i64 @ptrtoint1() {
  %p = alloca i32
  %dummy = alloca i64

  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  %i = ptrtoint ptr %p to i64
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)

  ret i64 %i
}

define i64 @ptrtoint2() {
  %p = alloca i32
  %dummy = alloca i64

  %i = ptrtoint ptr %p to i64
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)

  ret i64 %i
}
