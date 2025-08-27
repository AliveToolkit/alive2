define i1 @src(ptr %0, ptr byval(i32) %1) {
  %q = call ptr @f(ptr %0)
  %b = getelementptr inbounds i8, ptr %1, i64 1
  %r = getelementptr inbounds i8, ptr %q, i64 1
  %c = icmp eq ptr %r, %b
  ret i1 %c
}

define i1 @tgt(ptr %0, ptr byval(i32) %1) {
  %q = call ptr @f(ptr %0)
  ret i1 false
}

declare ptr @f(ptr)
