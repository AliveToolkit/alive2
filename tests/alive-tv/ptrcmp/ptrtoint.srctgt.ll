declare void @f(i8*)

define void @src(i8* %p) {
  %unused = ptrtoint i8* %p to i64
  %c = icmp eq i8* %p, null
  br i1 %c, label %A, label %B
A:
  ret void
B:
  call void @f(i8* %p)
  ret void
}

define void @tgt(i8* %p) {
  %unused = ptrtoint i8* %p to i64
  %c = icmp eq i8* %p, null
  br i1 %c, label %A, label %B
A:
  ret void
B:
  call void @f(i8* nonnull %p)
  ret void
}
