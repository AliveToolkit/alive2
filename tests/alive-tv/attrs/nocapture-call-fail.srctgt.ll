; ERROR: Source is more defined

define void @src(ptr captures(none) %p) {
  call ptr @g(ptr %p)
  ret void
}

define void @tgt(ptr captures(none) %p) {
  call ptr @g(ptr poison)
  ret void
}

declare ptr @g(ptr captures(none))
