; TEST-ARGS:

define i32 @f(i8 %a, i32 %b, i64 %c, i1 %d) {
  %x = zext i8 %a to i32
  %y = add i32 %x, %b
  %z = trunc i64 %c to i32
  %cc = icmp ult i32 %y, %z
  %cd = icmp eq i64 %c, -1
  %i = select i1 %cc, i1 %cd, i1 %d
  %r = sext i1 %i to i32
  ret i32 %r
}