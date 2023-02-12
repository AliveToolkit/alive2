define void @src() {
  store i8 0, ptr poison
  ret void
}

define void @tgt() {
  ret void
}

