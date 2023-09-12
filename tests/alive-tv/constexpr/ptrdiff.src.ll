target datalayout = "p:64:64:64"
@g = constant i8 0
@h = constant i8 0

define i64 @f() {
  ret i64 sub (i64 ptrtoint (ptr getelementptr inbounds (i8, ptr @g, i64 1) to i64), i64 ptrtoint (ptr @h to i64))
}
