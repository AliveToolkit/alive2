; Found by Alive2

define i1 @src(i8 %y) {
  %x = call i8 @gen8()
  %tmp0 = lshr i8 255, %y
  %tmp1 = and i8 %tmp0, %x
  %ret = icmp sgt i8 %x, %tmp1
  ret i1 %ret
}

define i1 @tgt(i8 %y) {
  %x = call i8 @gen8()
  %tmp0 = lshr i8 255, %y
  %1 = icmp sgt i8 %x, %tmp0
  ret i1 %1
}

declare i8 @gen8()

; ERROR: Value mismatch
