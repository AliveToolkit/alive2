; TEST-ARGS: -tgt-is-asm

declare void @g(ptr)

define void @src(ptr %x) {
  call void @g(ptr %x)
  ret void
}

define void @tgt(ptr %x) {
  call void @g(ptr %x)
  ret void
}
