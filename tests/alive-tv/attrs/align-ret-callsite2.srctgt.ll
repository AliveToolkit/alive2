define void @src() {
  %p = call i8* @g()
  load i8, i8* %p
  ret void
}

define void @tgt() {
  %p = call i8* @g()
  load i8, i8* %p, align 4
  ret void
}

declare align 4 i8* @g()
