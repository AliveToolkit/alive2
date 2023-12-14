; TEST-ARGS: -tgt-is-asm

define void @src(ptr %p) {
  store i8 undef, ptr %p
  ret void
}

define void @tgt(ptr %p) {
  ret void
}
