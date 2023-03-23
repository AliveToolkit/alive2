define ptr @i32_m1(ptr %0, i64 %1) {
  %_t = trunc i64 %1 to i32
  %3 = getelementptr inbounds i8, ptr %0, i32 -1
  store i32 %_t, ptr %3, align 4
  ret ptr %3
}