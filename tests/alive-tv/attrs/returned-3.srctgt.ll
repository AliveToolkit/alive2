define i32 @src(i32 returned %x) {
  %c = icmp eq i32 %x, 10
  br i1 %c, label %A, label %B
A:
  ret i32 11 ; UB
B:
  ret i32 0
}

define i32 @tgt(i32 returned %x) {
  %c = icmp eq i32 %x, 10
  br i1 %c, label %A, label %B
A:
  unreachable
B:
  ret i32 0
}
