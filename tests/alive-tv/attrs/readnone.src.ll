define void @f(ptr readnone %x) {
  load i8, ptr %x
  ret void
}
