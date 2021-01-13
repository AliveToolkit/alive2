@g = internal unnamed_addr constant i8* undef, align 8

define i8 @src()  {
  %a = alloca i8, align 8
  %v = load i8*, i8** @g
  ret i8 0
}

define i8 @tgt() {
  ret i8 0
}
