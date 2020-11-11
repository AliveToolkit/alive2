define i1 @src(i32 %c.3.i, i32 %d.292.2.i) {
   %tmp266.i = icmp slt i32 %c.3.i, %d.292.2.i     
   %tmp276.i = icmp eq i32 %c.3.i, %d.292.2.i 
   %sel_tmp80 = or i1 %tmp266.i, %tmp276.i 
   ret i1 %sel_tmp80
}

define i1 @tgt(i32 %c.3.i, i32 %d.292.2.i) {
  %sel_tmp187.demorgan.i = icmp ule i32 %c.3.i, %d.292.2.i
  ret i1 %sel_tmp187.demorgan.i
}

; ERROR: Value mismatch
