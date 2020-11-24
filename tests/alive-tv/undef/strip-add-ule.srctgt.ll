define i32 @src(i32 %x) {
  %c = icmp ule i32 0, %x
  br i1 %c, label %A, label %B
A:
  %x.fr = freeze i32 %x
  ret i32 %x.fr
B:
  ret i32 0
}

define i32 @tgt(i32 %x) {
  %c = icmp ule i32 0, %x
  br i1 %c, label %A, label %B
A:
  ret i32 %x
B:
  ret i32 0
}

; ERROR: Target's return value is more undefined
