define i8 @f() {
  %a = alloca i8
  %b = call i8 @g(i8* %a)
  store i8 3, i8* %a
  ret i8 %b
}

declare i8 @g(i8*)
