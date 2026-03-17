; EXPECT: ERROR: Source is more defined than target

define i8 @src() {
  %p = alloca i8
  call void @llvm.lifetime.start.p0(ptr %p)
  call void @llvm.lifetime.end.p0(ptr %p)
  %v = load i8, ptr %p
  ret i8 %v
}

define i8 @tgt() {
  unreachable
}
