; ERROR: Source is more defined

define void @src(i8* nocapture %p) {
  call i8* @g(i8* %p)
  ret void
}

define void @tgt(i8* nocapture %p) {
  call i8* @g(i8* poison)
  ret void
}

declare i8* @g(i8* nocapture)
