declare void @f(i8*)

define void @src(i8* %p) {
  %c = icmp eq i8* %p, null
  br i1 %c, label %A, label %B
A:
  call void @f(i8* %p)
  ret void
B:
  ret void
}

define void @tgt(i8* %p) {
  %c = icmp eq i8* %p, null
  br i1 %c, label %A, label %B
A:
  call void @f(i8* null)
  ret void
B:
  ret void
}
