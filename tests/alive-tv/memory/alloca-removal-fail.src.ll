define ptr @f_observed(i64 %size) {
  %items.addr = alloca i32, align 4
  %call = call noalias ptr @malloc(i64 %size)
  %i = ptrtoint ptr %call to i64
  ret ptr %call
}

; ERROR: Value mismatch
declare noalias ptr @malloc(i64)
