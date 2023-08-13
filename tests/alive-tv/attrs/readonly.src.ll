define void @f(ptr readonly %x) {
  store i8 0, ptr %x
  ret void
}
