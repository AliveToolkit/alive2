define i1 @src(ptr %p) {
  call void @f(ptr nonnull %p)
  %c = icmp eq ptr %p, null
  ret i1 %c
}

define i1 @tgt(ptr %p) {
  call void @f(ptr nonnull %p)
  ret i1 0
}

declare void @f(ptr noundef)
