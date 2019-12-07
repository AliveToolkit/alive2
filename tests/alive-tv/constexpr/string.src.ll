;target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
;target triple = "x86_64-apple-macosx10.15.0"

@.str = constant [6 x i8] c"hello\00", align 1
@aa = constant i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str, i32 0, i32 0)

define i8 @h() {
  %aa = load i8*, i8** @aa
  %c = load i8, i8* %aa
  ret i8 %c
}

define i8 @e() {
  %aa = load i8*, i8** @aa
  %p = getelementptr i8, i8* %aa, i32 1
  %c = load i8, i8* %p
  ret i8 %c
}

define i8 @l() {
  %aa = load i8*, i8** @aa
  %p = getelementptr i8, i8* %aa, i32 2
  %c = load i8, i8* %p
  ret i8 %c
}

define i8 @l2() {
  %aa = load i8*, i8** @aa
  %p = getelementptr i8, i8* %aa, i32 3
  %c = load i8, i8* %p
  ret i8 %c
}

define i8 @o() {
  %aa = load i8*, i8** @aa
  %p = getelementptr i8, i8* %aa, i32 4
  %c = load i8, i8* %p
  ret i8 %c
}

define i8 @zero() {
  %aa = load i8*, i8** @aa
  %p = getelementptr i8, i8* %aa, i32 5
  %c = load i8, i8* %p
  ret i8 %c
}


