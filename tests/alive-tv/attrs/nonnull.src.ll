define i1 @f1(i8* nonnull %p) {
  %a = ptrtoint i8* %p to i64
  %b = ptrtoint i8* %p to i64
  %c = icmp eq i64 %b, 0
  ret i1 %c
}

define i1 @f2(i8* nonnull %p) {
  ret i1 0
}
