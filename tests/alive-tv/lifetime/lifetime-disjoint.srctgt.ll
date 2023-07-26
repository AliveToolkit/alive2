declare void @llvm.lifetime.start.p0i8(i64, ptr)
declare void @llvm.lifetime.end.p0i8(i64, ptr)

define i32 @src() {
  %p = alloca i32
  %q = alloca i32

  %p0 = bitcast ptr %p to ptr
  %q0 = bitcast ptr %q to ptr
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p0)
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %q0)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %q0)

  %ip = ptrtoint ptr %p to i64
  %iq = ptrtoint ptr %q to i64
  %c = icmp eq i64 %ip, %iq
  br i1 %c, label %A, label %B
A:
  ret i32 1
B:
  ret i32 0
}

define i32 @tgt() {
  %p = alloca i32
  %q = alloca i32

  call void @llvm.lifetime.start.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %p)
  call void @llvm.lifetime.start.p0i8(i64 4, ptr %q)
  call void @llvm.lifetime.end.p0i8(i64 4, ptr %q)

  %ip = ptrtoint ptr %p to i64
  %iq = ptrtoint ptr %q to i64
  %c = icmp eq i64 %ip, %iq
  br i1 %c, label %A, label %B
A:
  unreachable
B:
  ret i32 0
}

; ERROR: Source is more defined than target
