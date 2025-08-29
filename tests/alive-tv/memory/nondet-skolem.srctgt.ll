; ERROR: Mismatch in memory

@p = global i8 0
@q = global i8 0

define void @src() {
  %c = freeze i1 poison
  br i1 %c, label %A, label %B
A:
  store i8 0, i8* @p
  ret void
B:
  store i8 0, i8* @q
  ret void
}

define void @tgt() {
  ret void
}
