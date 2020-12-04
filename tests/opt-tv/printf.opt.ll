; TEST-ARGS: -instcombine
@.str = constant [3 x i8] c"a\0A\00", align 1

define void @f() {
  call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0))
  ret void
}

declare i32 @printf(i8*, ...)

; ERROR: Invalid expr
