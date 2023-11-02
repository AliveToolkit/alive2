@g = global [3 x i8] zeroinitializer, align 1

define void @f() {
  call void @fn2(ptr @g)
  ret void
}

declare void @fn2(ptr)
