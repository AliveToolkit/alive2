@glb = global i8* null
declare i8* @f(i8* %p)

define i8 @src() {
  %p = alloca i8
  store i8 1, i8* %p

  %q = load i8*, i8** @glb
  %q2 = call i8* @f(i8* %q) ; cannot be %p
  store i8 2, i8* %q2

  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  store i8 1, i8* %p

  %q = load i8*, i8** @glb
  %q2 = call i8* @f(i8* %q) ; cannot be %p
  store i8 2, i8* %q2

  ret i8 1
}
