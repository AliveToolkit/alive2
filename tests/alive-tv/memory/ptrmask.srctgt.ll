define i1 @src(ptr align(8) %p0) {
  %p = getelementptr inbounds i8, ptr %p, i8 1
  %q = call ptr @llvm.ptrmask(ptr %p, i64 -8)
  %int = ptrtoint ptr %q to i64
  %r = icmp eq i64 %int, 0
  ret i1 %r
}

define i1 @tgt(ptr align(8) %p) {
  ret i1 true
}

declare ptr @llvm.ptrmask(ptr, i64)
