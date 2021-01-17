define i1 @src(i8* %p) {
  call void @f(i8* nonnull %p)
  %c = icmp eq i8* %p, null
  ret i1 %c
}

define i1 @tgt(i8* %p) {
  call void @f(i8* nonnull %p)
  ret i1 0
}

declare void @f(i8*)

; ERROR: Value mismatch
