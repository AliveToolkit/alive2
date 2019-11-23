; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @alloca_malloc() {
  %ptr = call noalias i8* @malloc(i64 1)
  %cmp = icmp eq i8* %ptr, null
  br i1 %cmp, label %BB1, label %BB2
BB1:
  ret i8 0
BB2:
  store i8 10, i8* %ptr
  %v = load i8, i8* %ptr
  call void @free(i8* %ptr)
  ret i8 %v
}

declare noalias i8* @malloc(i64)
declare void @free(i8*)

; If there is '%ptr == null` check guard, this optimization is invalid
; because source can safely return whereas alloca may raise OOM (which is UB).
; ERROR: Source is more defined than target
