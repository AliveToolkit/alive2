define void @src(ptr %p) {
  call void @g(ptr align(4) %p)
  load i8, ptr %p
  ret void
}

define void @tgt(ptr %p) {
  call void @g(ptr align(4) %p)
  load i8, ptr %p, align 8
  ret void
}

declare void @g(ptr align(8) noundef)
