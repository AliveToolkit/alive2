@x = constant i16 0
@y = constant i8* getelementptr inbounds (i8, i8* bitcast (i16* @x to i8*), i64 0)

define i8* @f() {
  %p = load i8*, i8** @y ; to relieve mismatch in memory error
  %a = bitcast i16* @x to i8*
  %b = getelementptr inbounds i8, i8* %a, i64 1
  ret i8* %b
}
