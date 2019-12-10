@a = constant i32 0
@x = constant { i8*, i8* } { i8* bitcast (i32* @a to i8*), i8* bitcast (i32* @a to i8*) }
@y = constant { i64, i64 } { i64 sub (i64 ptrtoint (i32* @a to i64), i64 1), i64 0 }

 define i64 @f() {
   %a = load { i8*, i8* }, { i8*, i8* }* @x
   %z = load { i64, i64 }, { i64, i64 }* @y
   %b = extractvalue {i64, i64} %z, 0
   ret i64 %b
 }
