define i8 @f() {
  %a = alloca i8
  %b = call i8 @g(ptr %a)
  store i8 3, ptr %a
  ret i8 %b
}

declare i8 @g(ptr)
