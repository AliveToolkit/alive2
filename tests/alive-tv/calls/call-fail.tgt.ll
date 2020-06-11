define i1 @f() {
  %p = alloca i8, align 1
  %call = call i8* @g(i8* %p)
  ret i1 false
}

declare i8* @g(i8*)
