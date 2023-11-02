target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i1 @f() {
  %ptr = alloca i8
  %i = ptrtoint ptr %ptr to i64
  %x = icmp eq i64 %i, 0
  ret i1 %x
}
