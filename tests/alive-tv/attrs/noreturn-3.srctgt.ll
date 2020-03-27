declare void @f() noreturn
declare void @g()

; This should hold, because src has tgt's behavior
define i8 @src() {
  %c = freeze i1 undef
  br i1 %c, label %A, label %B
A:
  call void @g()
  ret i8 0
B:
  call void @f()
  ret i8 0
}

define i8 @tgt() {
  call void @f()
  ret i8 0
}
