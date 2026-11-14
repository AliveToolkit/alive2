; %a and %c escape, %b doesn't: escaped local blocks are {0, 2}
; @g may return %c
declare void @f(ptr)
declare ptr @g(ptr)

define i1 @src() {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  call void @f(ptr %a)
  %p = call ptr @g(ptr %c)
  %r = icmp eq ptr %p, %c
  ret i1 %r
}

define i1 @tgt() {
  %a = alloca i8
  %b = alloca i8
  %c = alloca i8
  call void @f(ptr %a)
  %p = call ptr @g(ptr %c)
  ret i1 false
}

; ERROR: Value mismatch
