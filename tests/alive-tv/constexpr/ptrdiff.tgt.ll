target datalayout = "p:64:64:64"
@g = constant i8 0
@h = constant i8 0

define i64 @f() {
  ret i64 add (i64 sub (i64 ptrtoint (i8* @g to i64), i64 ptrtoint (i8* @h to i64)), i64 1)
}
