define i1 @f() {
  %p = alloca i8, align 1
  %call = call ptr @g(ptr %p)
  ret i1 false
}

declare ptr @g(ptr)
