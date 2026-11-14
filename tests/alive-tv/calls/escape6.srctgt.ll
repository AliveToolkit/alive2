; %a and %c escape, %b doesn't: escaped local blocks are {0, 2}
; @g may return %c, so the store to %p may clobber %c
declare void @f(ptr)
declare ptr @g(ptr)

define i8 @src() {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  call void @f(ptr %a)
  %p = call ptr @g(ptr %c)
  store i8 1, ptr %c
  store i8 2, ptr %p
  %v = load i8, ptr %c
  ret i8 %v
}

define i8 @tgt() {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  call void @f(ptr %a)
  %p = call ptr @g(ptr %c)
  store i8 1, ptr %c
  store i8 2, ptr %p
  ret i8 1
}

; ERROR: Value mismatch
