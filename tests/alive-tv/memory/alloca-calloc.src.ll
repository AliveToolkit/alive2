; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8 @alloca_calloc() {
  %ptr = call noalias ptr @calloc(i64 1, i64 1)
  ; If there is '%ptr == null` check guard, this optimization is invalid
  ; because source can safely return whereas alloca may raise OOM (which is UB).
  store i8 20, ptr %ptr
  %v = load i8, ptr %ptr
  call void @free(ptr %ptr)
  ret i8 %v
}

declare noalias ptr @calloc(i64, i64)
declare void @free(ptr)
