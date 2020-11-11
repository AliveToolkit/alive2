; Found by Alive2

define i1 @src(i8 %x, i8 %y) {
  %tmp0 = lshr i8 255, %y
  %tmp1 = and i8 %tmp0, %x
  %ret = icmp sge i8 %tmp1, %x
  ret i1 %ret
}

define i1 @tgt(i8 %x, i8 %y) {
  %tmp0 = lshr i8 255, %y
  %1 = icmp sge i8 %tmp0, %x
  ret i1 %1
}

; ERROR: Value mismatch
