@g = internal unnamed_addr constant ptr poison, align 8

define i8 @src()  {
  %a = alloca i8, align 8
  %v = load ptr, ptr @g
  ret i8 0
}

define i8 @tgt() {
  ret i8 0
}
