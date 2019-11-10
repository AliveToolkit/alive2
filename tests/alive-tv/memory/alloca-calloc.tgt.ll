; TEST-ARGS: -smt-to=9000
; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @alloca_calloc() {
  %ptr = alloca i8
  store i8 20, i8* %ptr
  %v = load i8, i8* %ptr
  ret i8 %v
}

declare noalias i8* @calloc(i64, i64)
declare void @free(i8*)
