; ERROR: Value mismatch

; The mirror image of denormal-input-flush.srctgt.ll: LangRef says of the
; output mode that "It is not mandated that flushing to zero occurs", so a
; denormal result may or may not be flushed. The target may return a subnormal,
; so it is not a refinement of the source, which always returns zero.

define float @src() denormal_fpenv(positivezero|ieee) {
  ret float 0.000000e+00
}

define float @tgt() denormal_fpenv(positivezero|ieee) {
; 0x3810000000000000 is 0x1p-126, the smallest normal float; halving it
; produces a subnormal result
  %v = fdiv float 0x3810000000000000, 2.000000e+00
  ret float %v
}
