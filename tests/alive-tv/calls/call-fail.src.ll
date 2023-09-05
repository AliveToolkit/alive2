; ERROR: Value mismatch

define i1 @f() {
  %p = alloca i8, align 1
  %call = call ptr @g(ptr %p)
  %c = icmp eq ptr %p, %call
  ret i1 %c
}

declare ptr @g(ptr)
