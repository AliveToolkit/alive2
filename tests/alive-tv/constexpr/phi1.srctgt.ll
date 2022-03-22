@a = external global [2 x i8]
@b = external global i8

define i8 @src(i1 %c) {
entry:
  br i1 %c, label %then, label %exit

then:
  br label %exit

exit:
  %p = phi i8 [ 0, %entry ], [ srem (i8 1, i8 zext (i1 icmp eq (i8* getelementptr inbounds ([2 x i8], [2 x i8]* @a, i64 0, i64 1), i8* @b) to i8)), %then ]
  ret i8 %p
}

define i8 @tgt(i1 %c) {
entry:
  br i1 %c, label %then, label %exit

then:
  %rem = srem i8 1, 0
  br label %exit

exit:
  %p = phi i8 [ 0, %entry ], [ %rem, %then ]
  ret i8 %p
}
