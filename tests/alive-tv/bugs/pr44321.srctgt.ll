; Found by Alive2

define ptr @src(ptr %x, i64 %y, i64 %z) {
  %gep = getelementptr [7 x i32], ptr %x, i64 %y, i64 %z
  ret ptr %gep
}

define ptr @tgt(ptr %x, i64 %y, i64 %z) {
  %gep = getelementptr <7 x i32>, ptr %x, i64 %y, i64 %z
  ret ptr %gep
}

; ERROR: Value mismatch
