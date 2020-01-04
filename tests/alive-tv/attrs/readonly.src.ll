define void @f(i8* readonly %x) {
  store i8 0, i8* %x
  ret void
}
