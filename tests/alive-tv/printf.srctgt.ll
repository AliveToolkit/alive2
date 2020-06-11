; TEST-ARGS: -io-nobuiltin
@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1

define i32 @src() {
entry:
  %r = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i32 10)
  ret i32 %r
}

define i32 @tgt() {
entry:
  %r = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i32 10)
  ret i32 %r
}

declare i32 @printf(i8* nocapture readonly, ...)
