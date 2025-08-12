declare void @llvm.lifetime.start.p0(ptr)
declare void @llvm.lifetime.end.p0(ptr)

define i32 @src() {
  %p = alloca i32
  %q = alloca i32

  call void @llvm.lifetime.start.p0(ptr %p)
  call void @llvm.lifetime.end.p0(ptr %p)
  call void @llvm.lifetime.start.p0(ptr %q)
  call void @llvm.lifetime.end.p0(ptr %q)

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

  call void @llvm.lifetime.start.p0(ptr %p)
  call void @llvm.lifetime.end.p0(ptr %p)
  call void @llvm.lifetime.start.p0(ptr %q)
  call void @llvm.lifetime.end.p0(ptr %q)

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
