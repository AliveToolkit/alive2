define i8 @f1() {
  %a = alloca i8
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

define i8 @f2(i8* %a) {
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

define i8 @f3(i8* %a) {
  store i8 undef, i8* %a
  %b = call i8 @g(i8* %a)
  ret i8 %b
}

declare i8 @g(i8*)
