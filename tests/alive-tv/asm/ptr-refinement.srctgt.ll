; TEST-ARGS: -tgt-is-asm
; SKIP-IDENTITY

define ptr @src(ptr %0, ptr %1) {
  %3 = ptrtoint ptr %0 to i64
  %4 = ptrtoint ptr %1 to i64
  %5 = sub i64 %4, %3
  %6 = getelementptr i8, ptr %0, i64 %5
  ret ptr %6
}

define ptr @tgt(ptr %0, ptr %1) {
  ret ptr %1
}
