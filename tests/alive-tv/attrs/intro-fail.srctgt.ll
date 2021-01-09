; ERROR: Source is more defined than target

define void @src(i8* %p) {
  store i8 1, i8* %p
  ret void
}

define void @tgt(i8* %p) readonly {
  store i8 1, i8* %p
  ret void
}
