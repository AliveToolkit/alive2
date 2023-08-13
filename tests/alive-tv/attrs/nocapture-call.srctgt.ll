define void @src(ptr nocapture %p) {
  call ptr @g(ptr %p)
  ret void
}

define void @tgt(ptr nocapture %p) {
  call ptr @g(ptr poison)
  ret void
}

declare ptr @g(ptr)
