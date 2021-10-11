define i8 @f() {
  %a = alloca i8
  store i8 3, i8* %a
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

declare i8 @g(i8*)

; ERROR: Source is more defined than target
