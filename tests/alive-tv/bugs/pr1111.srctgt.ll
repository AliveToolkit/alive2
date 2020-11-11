; This is incorrect when %X = NaN
define i1 @src(double %X) {
  %tmp = fcmp une double %X, %X
  ret i1 %tmp
}

define i1 @tgt(double %X) {
  ret i1 false
}

; ERROR: Value mismatch
