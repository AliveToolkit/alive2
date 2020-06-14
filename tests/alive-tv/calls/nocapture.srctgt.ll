declare void @f_nocapture(i8* nocapture %p)
declare i8* @g()

define i8 @src() {
  %p = alloca i8
  store i8 1, i8* %p
  call void @f_nocapture(i8* %p)
  %q = call i8* @g()
  store i8 2, i8* %q
  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  store i8 1, i8* %p
  call void @f_nocapture(i8* %p)
  %q = call i8* @g()
  store i8 2, i8* %q
  ret i8 1
}
