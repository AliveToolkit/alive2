define void @src(ptr %p) {
  %c = icmp ne ptr %p, null
  br i1 %c, label %A, label %B
A:
  call void @f(ptr %p)
  ret void
B:
  ret void
}

define void @tgt(ptr %p) {
  %c = icmp ne ptr %p, null
  br i1 %c, label %A, label %B
A:
  call void @f(ptr nonnull %p)
  ret void
B:
  ret void
}

declare void @f(ptr)
