; TEST-ARGS: -disable-undef-input
; ERROR: Value mismatch
; CHECK: %m = #x7e00

define half @src(i1 %x, half %y) {
  %z = uitofp i1 %x to half
  %m = fmul nsz half %y, %z
  ret half %m
}

define half @tgt(i1 %x, half %y) {
  %m = select i1 %x, half %y, half 0.000000e+00
  ret half %m
}
