declare i32 @external()

@g = global i8 0
@h = global i8 0

define i32 @src() {
  store i8 0, i8* @g
  store i8 1, i8* @h
  %call = call i32 @external(), !range !0
  %urem = udiv i32 %call, 3
  ret i32 %urem
}

define i32 @tgt() {
  store i8 1, i8* @h
  store i8 0, i8* @g
  %call = call i32 @external(), !range !0
  ret i32 0
}

!0 = !{i32 0, i32 3}
