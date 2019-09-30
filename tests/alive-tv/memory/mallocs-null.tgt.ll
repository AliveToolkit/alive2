; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @malloc_null() {
  %ptr1 = call noalias i8* @malloc(i64 1)
  %ptr2 = call noalias i8* @malloc(i64 1)
  %ptr3 = call noalias i8* @malloc(i64 1)
  %ptr4 = call noalias i8* @malloc(i64 1)
  %ptr5 = call noalias i8* @malloc(i64 1)
  %ptr6 = call noalias i8* @malloc(i64 1)
  %ptr7 = call noalias i8* @malloc(i64 1)
  ; 2 + 4 + 6 + 8 + 10 + 12 + 14
  ret i64 56
}

declare noalias i8* @malloc(i64)
