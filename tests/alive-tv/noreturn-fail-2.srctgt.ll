declare void @f() noreturn

define i8 @src() {
  call void @f()
  ret i8 0
}

define i8 @tgt() {
  ret i8 0
}

; ERROR: Mismatch in memory
