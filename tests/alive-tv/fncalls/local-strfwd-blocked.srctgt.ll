declare void @f()
@escape = global i8* null

define i8 @src() {
  %p = alloca i8
  store i8 1, i8* %p
  store i8* %p, i8** @escape
  call void @f()
  %v = load i8, i8* %p
  ret i8 %v
}

define i8 @tgt() {
  %p = alloca i8
  store i8 1, i8* %p
  store i8* %p, i8** @escape
  call void @f()
  ret i8 1
}

; ERROR: Value mismatch
