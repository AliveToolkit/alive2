define i1 @f1(ptr nonnull %p) {
  %a = ptrtoint ptr %p to i64
  %b = ptrtoint ptr %p to i64
  %c = icmp eq i64 %b, 0
  ret i1 %c
}

define i1 @f2(ptr nonnull %p) {
  ret i1 0
}
