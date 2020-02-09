define void @f(i8* readnone %x) {
  load i8, i8* %x
  ret void
}
