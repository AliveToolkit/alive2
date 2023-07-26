; Found by Alive2

define ptr @src(ptr %p, ptr %q) {
  %i = ptrtoint ptr %p to i64
  %j = ptrtoint ptr %q to i64
  %diff = sub i64 %j, %i
  %p2 = getelementptr i8, ptr %p, i64 %diff
  ret ptr %p2
}

define ptr @tgt(ptr %p, ptr %q) {
  ret ptr %q
}

; ERROR: Value mismatch
