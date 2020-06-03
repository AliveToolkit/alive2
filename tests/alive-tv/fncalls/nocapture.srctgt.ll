declare void @f_nocapture(i8* nocapture %p)
declare void @g()

define i8 @src() {
  %p = alloca i8
  store i8 1, i8* %p
  call void @f_nocapture(i8* %p)
  call void @g()
  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  store i8 1, i8* %p
  call void @f_nocapture(i8* %p)
  call void @g()
  ret i8 2
}

; ERROR: Value mismatch
