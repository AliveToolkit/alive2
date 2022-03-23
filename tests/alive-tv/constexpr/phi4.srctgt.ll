@e = external global [2 x i32]
@a = external global i32

define <2 x i8> @src(i1 %c, i8* %ptr) {
entry:
  br i1 %c, label %then, label %exit

then:
  %srem = srem i8 1, zext (i1 icmp eq (i32* getelementptr inbounds ([2 x i32], [2 x i32]* @e, i64 0, i64 1), i32* @a) to i8)
  %ins = insertelement <2 x i8> zeroinitializer, i8 %srem, i64 0
  br label %exit

exit:
  %p = phi <2 x i8> [ zeroinitializer, %entry ], [ %ins, %then ]
  %q = phi i8 [ 0, %entry ], [ 2, %then ]
  store i8 %q, i8* %ptr
  ret <2 x i8> %p
}

define <2 x i8> @tgt(i1 %c, i8* %ptr) {
entry:
  br i1 %c, label %then, label %exit

then:
  br label %exit

exit:
  %p = phi <2 x i8> [ zeroinitializer, %entry ], [ <i8 srem (i8 1, i8 zext (i1 icmp eq (i32* getelementptr inbounds ([2 x i32], [2 x i32]* @e, i64 0, i64 1), i32* @a) to i8)), i8 0>, %then ]
  %q = phi i8 [ 0, %entry ], [ 2, %then ]
  store i8 %q, i8* %ptr
  ret <2 x i8> %p
}
