@.str = constant [6 x i8] c"hello\00", align 1
@aa = constant ptr getelementptr inbounds ([6 x i8], ptr @.str, i32 0, i32 0)

define i8 @h() {
  %aa = load ptr, ptr @aa
  %c = load i8, ptr %aa
  ret i8 %c
}

define i8 @e() {
  %aa = load ptr, ptr @aa
  %p = getelementptr i8, ptr %aa, i32 1
  %c = load i8, ptr %p
  ret i8 %c
}

define i8 @l() {
  %aa = load ptr, ptr @aa
  %p = getelementptr i8, ptr %aa, i32 2
  %c = load i8, ptr %p
  ret i8 %c
}

define i8 @l2() {
  %aa = load ptr, ptr @aa
  %p = getelementptr i8, ptr %aa, i32 3
  %c = load i8, ptr %p
  ret i8 %c
}

define i8 @o() {
  %aa = load ptr, ptr @aa
  %p = getelementptr i8, ptr %aa, i32 4
  %c = load i8, ptr %p
  ret i8 %c
}

define i8 @zero() {
  %aa = load ptr, ptr @aa
  %p = getelementptr i8, ptr %aa, i32 5
  %c = load i8, ptr %p
  ret i8 %c
}
