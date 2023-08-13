define void @src(ptr %p) {
  call void @f(ptr null)
  ret void
}

define void @tgt(ptr %p) {
  unreachable
}

declare void @f(ptr nonnull)

; ERROR: Source is more defined than target
