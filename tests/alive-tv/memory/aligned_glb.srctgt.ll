@0 = external constant i8

define i1 @src(ptr %0) {
  %2 = getelementptr inbounds i8, ptr %0, i64 1
  call void @fn(ptr %2, ptr @0)
  %3 = icmp eq ptr %0, null
  ret i1 %3
}

define i1 @tgt(ptr %0) {
  %2 = getelementptr inbounds i8, ptr %0, i64 1
  call void @fn(ptr nonnull %2, ptr @0)
  %3 = icmp eq ptr %0, null
  ret i1 %3
}

declare void @fn(ptr, ptr)
