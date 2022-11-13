; ERROR: Source is more defined

define void @src(i8 %var, ptr %p) memory(write) {
  ret void
}

define void @tgt(i8 %var, ptr %p) memory(write) {
  %x = load i8, ptr %p
  ret void
}
