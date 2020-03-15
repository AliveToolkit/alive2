; https://bugs.llvm.org/show_bug.cgi?id=32949

define i1 @src(i8 %x) {
  %div1 = sdiv exact i8 1, %x
  %div2 = sdiv exact i8 -1, %x
  %icmp = icmp ult i8 %div1, %div2
  ret i1 %icmp
}

define i1 @tgt(i8 %x) {
  ret i1 true
}

; ERROR: Value mismatch
