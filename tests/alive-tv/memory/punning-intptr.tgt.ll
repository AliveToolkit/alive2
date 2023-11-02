target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define ptr @int_ptr_punning(ptr %ptr, i64 %n) {
  store i8 1, ptr %ptr
  %poison = getelementptr inbounds i8, ptr null, i64 1
  ret ptr %poison
}
