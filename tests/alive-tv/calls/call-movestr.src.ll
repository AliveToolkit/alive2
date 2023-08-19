; ERROR: Source is more defined

define i8 @f(ptr %a) {
  store i8 3, ptr %a
  %b = call i8 @g(ptr %a)
  ret i8 %b
}

declare i8 @g(ptr)
