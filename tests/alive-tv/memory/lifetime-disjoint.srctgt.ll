declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)

define i32 @src() {
  %p = alloca i32
  %q = alloca i32

  %p0 = bitcast i32* %p to i8*
  %q0 = bitcast i32* %q to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %q0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %q0)

  %ip = ptrtoint i32* %p to i64
  %iq = ptrtoint i32* %q to i64
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

  %p0 = bitcast i32* %p to i8*
  %q0 = bitcast i32* %q to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %p0)
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %q0)
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %q0)

  %ip = ptrtoint i32* %p to i64
  %iq = ptrtoint i32* %q to i64
  %c = icmp eq i64 %ip, %iq
  br i1 %c, label %A, label %B
A:
  unreachable
B:
  ret i32 0
}

; ERROR: Source is more defined than target
