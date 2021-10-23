; ERROR: Source is more defined

define void @src(i8 %var, i8* %p) writeonly {
  ret void
}

define void @tgt(i8 %var, i8* %p) writeonly {
  %x = load i8, i8* %p
  ret void
}
