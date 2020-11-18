define float @src(i26 %x, i26 %y) {
  %xx = or i26 %x, 42074059
  %yy = and i26 %y, 14942208
  %t0 = sitofp i26 %xx to float
  %t1 = sitofp i26 %yy to float
  %rr = fadd float %t0, %t1
  ret float %rr
}

define float @tgt(i26 %x, i26 %y) {
  %xx = or i26 %x, -25034805
  %yy = and i26 %y, 14942208
  %addconv = add nsw i26 %xx, %yy
  %rr = sitofp i26 %addconv to float
  ret float %rr
}

; ERROR: Value mismatch
