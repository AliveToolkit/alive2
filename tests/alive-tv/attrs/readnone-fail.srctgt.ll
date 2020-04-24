; TEST-ARGS: -disable-undef-input
; This test checks LangRef's following statement:
; """
; this attribute indicates that the function does not dereference that pointer
; argument, even though it may read or write the memory that the pointer points
; to if accessed through other pointers.
; """
define i8 @src(i8* readnone %x, i8* %y) {
  %c = icmp eq i8* %x, %y
  br i1 %c, label %A, label %EXIT
A:
  %ret = load i8, i8* %y
  ret i8 %ret
EXIT:
  ret i8 0
}

define i8 @tgt(i8* readnone %x, i8* %y) {
  ret i8 0
}

; ERROR: Value mismatch
