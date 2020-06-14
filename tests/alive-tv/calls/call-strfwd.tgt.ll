define i8 @f(i8* %p) {
  %b = call i8 @g(i8* %p)
  ret i8 %b
}

declare i8 @g(i8*)
