target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define ptr @int_ptr_punning(ptr %ptr, i64 %n) {
  store i8 1, ptr %ptr
  %n2 = and i64 %n, 7
  %n3 = sub i64 0, %n2
  %ptr_2 = getelementptr i8, ptr %ptr, i64 %n3
  %v = load ptr, ptr %ptr_2
  ; %v is poison.
  ret ptr %v
}
