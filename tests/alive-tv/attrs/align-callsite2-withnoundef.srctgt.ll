; ERROR: Source is more defined than target

define void @src(ptr %p) {
  call void @g(ptr align(4) %p)
  load i8, ptr %p
  ret void
}

define void @tgt(ptr %p) {
  call void @g(ptr align(4) %p)
  load i8, ptr %p, align 4
  ret void
}

declare void @g(ptr noundef)
