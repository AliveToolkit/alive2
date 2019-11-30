@x = constant i16 0
@y = constant i8* getelementptr inbounds (i8, i8* bitcast (i16* @x to i8*), i64 0)

define i8* @f() {
  %p = load i8*, i8** @y
  ret i8* %p
}

; ERROR: Value mismatch
