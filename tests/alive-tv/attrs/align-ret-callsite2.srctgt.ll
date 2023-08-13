define void @src() {
  %p = call ptr @g()
  load i8, ptr %p
  ret void
}

define void @tgt() {
  %p = call ptr @g()
  load i8, ptr %p, align 4
  ret void
}

declare align 4 ptr @g()
