declare i8* @f(i8*, i8*)

define i8 @src() {
  %p = alloca i8
  %q = alloca i8
  store i8 1, i8* %p
  store i8 2, i8* %q
  %r = call i8* @f(i8* %p, i8* %q)
  store i8 3, i8* %r
  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  %q = alloca i8
  store i8 2, i8* %p
  store i8 1, i8* %q
  %r = call i8* @f(i8* %q, i8* %p)
  store i8 3, i8* %r
  %v = load i8, i8* %q
  ret i8 %v
}
