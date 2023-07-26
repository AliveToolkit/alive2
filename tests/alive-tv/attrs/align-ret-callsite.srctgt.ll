define void @src() {
  %p = call align 4 ptr @g()
  load i8, ptr %p
  ret void
}

define void @tgt() {
  %p = call align 4 ptr @g()
  load i8, ptr %p, align 4
  ret void
}

declare ptr @g()
