define i8 @f(ptr %p) {
  %b = call i8 @g(ptr %p)
  ret i8 %b
}

declare i8 @g(ptr)
