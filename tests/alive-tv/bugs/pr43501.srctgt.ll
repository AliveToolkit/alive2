; Found by Alive2

define ptr @src(ptr %A) {
  %c = bitcast ptr %A to ptr
  ret ptr %c
}

define ptr @tgt(ptr %A) {
  %c = getelementptr inbounds [9 x [4 x float]], ptr %A, i64 0, i64 0
  ret ptr %c
}
