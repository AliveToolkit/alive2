declare void @f(i8*, i8*)

define void @src() {
  %p = alloca i8
  %q = alloca i8
  store i8 1, i8* %p
  store i8 2, i8* %q
  call void @f(i8* %p, i8* %q)
  ret void
}

define void @tgt() {
  %p = alloca i8
  %q = alloca i8
  store i8 2, i8* %p
  store i8 1, i8* %q
  call void @f(i8* %q, i8* %p)
  ret void
}
