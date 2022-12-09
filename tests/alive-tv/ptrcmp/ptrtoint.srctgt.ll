declare void @f(ptr)

define void @src(ptr %p) {
  %unused = ptrtoint ptr %p to i64
  %c = icmp eq ptr %p, null
  br i1 %c, label %A, label %B
A:
  ret void
B:
  call void @f(ptr %p)
  ret void
}

define void @tgt(ptr %p) {
  %unused = ptrtoint ptr %p to i64
  %c = icmp eq ptr %p, null
  br i1 %c, label %A, label %B
A:
  ret void
B:
  call void @f(ptr nonnull %p)
  ret void
}
