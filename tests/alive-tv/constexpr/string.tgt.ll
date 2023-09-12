@.str = constant [6 x i8] c"hello\00", align 1
@aa = constant ptr getelementptr inbounds ([6 x i8], ptr @.str, i32 0, i32 0)

define i8 @h() {
  ret i8 104
}

define i8 @e() {
  ret i8 101
}

define i8 @l() {
  ret i8 108
}

define i8 @l2() {
  ret i8 108
}

define i8 @o() {
  ret i8 111
}

define i8 @zero() {
  ret i8 0
}
