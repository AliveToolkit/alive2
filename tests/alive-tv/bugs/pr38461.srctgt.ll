; Found by Alive2
; The PR had several function pairs.
; I reproduced the first one which was a concrete IR program

define i2 @src(i2, i1, i4, i2, i1, i4, i2, i1, i4, i2, i1, i4) {
  %13 = urem i2 %0, -1
  ret i2 %13
}

define i2 @tgt(i2, i1, i4, i2, i1, i4, i2, i1, i4, i2, i1, i4) {
  %13 = icmp eq i2 %0, -1
  %14 = zext i1 %13 to i2
  %15 = add i2 %14, %0
  ret i2 %15
}

; ERROR: Value mismatch
