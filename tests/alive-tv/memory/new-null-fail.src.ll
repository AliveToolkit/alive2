; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @malloc_null() {
  %ptr = call noalias i8* @_Znwm(i64 1)
  %i = ptrtoint i8* %ptr to i64
  ret i64 %i
}

declare i8* @_Znwm(i64)

; ERROR: Value mismatch
