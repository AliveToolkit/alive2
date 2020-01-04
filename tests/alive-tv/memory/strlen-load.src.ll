target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i1 @f(i8* %p) {
  %l = call i64 @strlen(i8* %p)
  %c = icmp eq i64 %l, 0
  ret i1 %c
}
; TODO
; ERROR: Timeout

declare i64 @strlen(i8*)
