; This test checks LangRef's following statement:
; """
; this attribute indicates that the function does not dereference that pointer
; argument, even though it may read or write the memory that the pointer points
; to if accessed through other pointers.
; """
define i8 @src(ptr readnone %x, ptr %y) {
  %c = icmp eq ptr %x, %y
  br i1 %c, label %A, label %EXIT
A:
  %ret = load i8, ptr %y
  ret i8 %ret
EXIT:
  ret i8 0
}

define i8 @tgt(ptr readnone %x, ptr %y) {
  ret i8 0
}

; ERROR: Value mismatch
