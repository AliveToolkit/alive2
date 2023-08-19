@a = constant i32 0
@x = constant { ptr, ptr } { ptr @a, ptr @a }
@y = constant { i64, i64 } { i64 sub (i64 ptrtoint (ptr @a to i64), i64 1), i64 0 }

 define i64 @f() {
   %a = load { ptr, ptr }, ptr @x
   %z = load { i64, i64 }, ptr @y
   %b = extractvalue {i64, i64} %z, 0
   ret i64 %b
 }
