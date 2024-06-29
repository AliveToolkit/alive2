declare void @f()
declare void @g(ptr)

define void @src(ptr noundef %p) {
  call void @f()
  call void %p(ptr @f)
  ret void
}

define void @tgt(ptr noundef %p) {
  call void @f()
  %cmp = icmp eq ptr %p, @g
  br i1 %cmp, label %then, label %else

then:
  call void @g(ptr @f)
  ret void

else:
  call void %p(ptr @f)
  ret void
}
