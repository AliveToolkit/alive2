; ERROR: Value mismatch

define i1 @f() {
  %p = alloca i8, align 1
  %call = call i8* @g(i8* %p)
  %c = icmp eq i8* %p, %call
  ret i1 %c
}

declare i8* @g(i8*)
