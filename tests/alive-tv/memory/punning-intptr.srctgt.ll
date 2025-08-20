target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @src(ptr %ptr) {
  %v = load ptr, ptr %ptr
  %r = ptrtoaddr ptr %v to i64
  ret i64 %r
}

define i64 @tgt(ptr %ptr) {
  %v = load i64, ptr %ptr
  ret i64 %v
}
