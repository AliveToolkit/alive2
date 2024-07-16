declare void @varargs(ptr, ...)
declare void @g(ptr, ptr)

define void @src(ptr noundef %p) {
  call void (ptr, ...) @varargs(ptr null, ptr null)
  call void %p(ptr null, ptr @varargs)
  ret void
}

define void @tgt(ptr noundef %p) {
  call void (ptr, ...) @varargs(ptr null, ptr null)
  %cmp = icmp eq ptr %p, @g
  br i1 %cmp, label %then, label %else

then:
  call void @g(ptr null, ptr @varargs)
  ret void

else:
  call void %p(ptr null, ptr @varargs)
  ret void
}
