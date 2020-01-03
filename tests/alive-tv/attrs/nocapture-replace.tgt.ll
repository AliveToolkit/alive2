define i8* @f6(i8* nocapture %p, i8* %q) {
  %c = icmp eq i8* %p, %q
  br i1 %c, label %A, label %B
A:
  ret i8* %p
B:
  ret i8* null
}
