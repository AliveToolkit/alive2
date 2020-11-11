define i8 @src(i8 %p) {
entry:
  %v3 = zext i8 %p to i16
  %v4 = mul i16 %v3, 71
  %v5 = lshr i16 %v4, 8
  %v6 = trunc i16 %v5 to i8
  %v7 = sub i8 %p, %v6
  %v8 = lshr i8 %v7, 1
  %v9 = zext i8 %p to i16
  %v10 = mul i16 %v9, 71
  %v11 = lshr i16 %v10, 8
  %v12 = trunc i16 %v11 to i8
  %v13 = add i8 %v12, %v8
  %v14 = lshr i8 %v13, 7
  ret i8 %v14
}

define i8 @tgt(i8 %p) {
	ret i8 0
}
; ERROR: Value mismatch
