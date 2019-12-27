declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

define i8 @gep1() {
  %p = alloca i32

  %p0 = bitcast i32* %p to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  %p1 = getelementptr i8, i8* %p0, i32 1
  store i8 10, i8* %p1
  %v = load i8, i8* %p1
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)

  ret i8 %v
}

define i8 @gep2() {
  %p = alloca i32

  %p0 = bitcast i32* %p to i8*
  %p1 = getelementptr i8, i8* %p0, i32 1
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  store i8 10, i8* %p1
  %v = load i8, i8* %p1
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)

  ret i8 %v
}

define i64 @ptrtoint1() {
  %p = alloca i32
  %dummy = alloca i64

  %p0 = bitcast i32* %p to i8*
  %i = ptrtoint i32* %p to i64
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)

  ret i64 %i
}

define i64 @ptrtoint2() {
  %p = alloca i32
  %dummy = alloca i64

  %p0 = bitcast i32* %p to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  %i = ptrtoint i32* %p to i64
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)

  ret i64 %i
}
