define i8* @src(i8* %p) {
  %c = icmp eq i8* %p, null
  br i1 %c, label %A, label %B
A:
  ret i8* %p
B:
  ret i8* null
}

define i8* @tgt(i8* %p) {
  ret i8* null
}
