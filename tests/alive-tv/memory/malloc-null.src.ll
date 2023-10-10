; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i1 @malloc_null() {
  %ptr = call ptr @malloc(i64 1)
  %i = ptrtoint ptr %ptr to i64
  %eq = icmp eq i64 %i, 0
  ret i1 %eq
}

declare noalias ptr @malloc(i64)
