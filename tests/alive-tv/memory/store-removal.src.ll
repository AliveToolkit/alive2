; TEST-ARGS: -disable-poison-input -disable-undef-input

define void @f(i8* %p) {
  %v = load i8, i8* %p
  store i8 %v, i8* %p
  ret void
}

define void @f2(i8** %p) {
  %v = load i8*, i8** %p
  store i8* %v, i8** %p
  ret void
}
