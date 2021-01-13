@g = internal unnamed_addr constant i8* undef, align 8

define i1 @src()  {
  %a = alloca i8, align 8
  %v = load i8*, i8** @g
  %c = icmp eq i8* %a, %v
  ret i1 %c
}

define i1 @tgt() {
  ret i1 false
}
