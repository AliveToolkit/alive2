define void @src(i8* %p) {
  %c = icmp ne i8* %p, null
  br i1 %c, label %A, label %B
A:
  call void @f(i8* %p)
  ret void
B:
  ret void
}

define void @tgt(i8* %p) {
  %c = icmp ne i8* %p, null
  br i1 %c, label %A, label %B
A:
  call void @f(i8* nonnull %p)
  ret void
B:
  ret void
}

declare void @f(i8*)
