; https://reviews.llvm.org/D95238

define i8 @src() {
  ret i8 0
}

define i8 @tgt() {
  %p = alloca [2 x i8]
  store [2 x i8] [i8 0, i8 0], ptr %p
  %idx = and i64 undef, 1
  %p2 = getelementptr [2 x i8], ptr %p, i64 0, i64 %idx
  %v = load i8, ptr %p2
  ret i8 %v
}

; ERROR: Source is more defined than target
