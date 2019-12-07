;target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
;target triple = "x86_64-apple-macosx10.15.0"

@.str = constant [6 x i8] c"hello\00", align 1
@aa = constant i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str, i32 0, i32 0)

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


