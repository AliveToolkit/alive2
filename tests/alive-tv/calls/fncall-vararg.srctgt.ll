; ERROR: Source is more defined than target

target triple = "x86_64-unknown-linux-gnu"
declare void @g(ptr, ...)

define void @src() {
  tail call void (ptr, ...) @g(ptr null, i32 0)
  ret void
}

define void @tgt() {
  tail call void (ptr, ...) @g(ptr null)
  ret void
}
