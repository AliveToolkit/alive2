; ERROR: Source is more defined

@glb = external global i8, align 1

define void @src(i8 %var) writeonly {
  ret void
}

define void @tgt(i8 %var) writeonly {
  %x = load i8, i8* @glb
  ret void
}
