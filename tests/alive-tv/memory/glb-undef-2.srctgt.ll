@g = internal unnamed_addr constant ptr poison, align 8

define i1 @src()  {
  %a = alloca i8, align 8
  %v = load ptr, ptr @g
  %c = icmp eq ptr %a, %v
  ret i1 %c
}

define i1 @tgt() {
  ret i1 false
}
