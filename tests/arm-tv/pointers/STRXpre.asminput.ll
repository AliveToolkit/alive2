define zeroext i32 @trunc_(i64 zeroext %0, i64 zeroext %1, i64 %2, i64 %3) #0 {
  %_t2 = trunc i64 %2 to i32
  %_t1 = trunc i64 %1 to i16
  %_t = trunc i64 %0 to i8
  %5 = alloca i8, align 1
  %6 = alloca i16, align 2
  %7 = alloca i32, align 4
  %8 = alloca i64, align 8
  store i8 %_t, ptr %5, align 1
  store i16 %_t1, ptr %6, align 2
  store i32 %_t2, ptr %7, align 4
  store i64 %3, ptr %8, align 8
  %9 = load i64, ptr %8, align 8
  %10 = trunc i64 %9 to i32
  store i32 %10, ptr %7, align 4
  %11 = load i32, ptr %7, align 4
  %12 = trunc i32 %11 to i16
  store i16 %12, ptr %6, align 2
  %13 = load i16, ptr %6, align 2
  %14 = trunc i16 %13 to i8
  store i8 %14, ptr %5, align 1
  %15 = load i8, ptr %5, align 1
  %16 = zext i8 %15 to i32
  ret i32 %16
}