; Converse of denormal-fptrunc-result-type.srctgt.ll: the f32 result of this
; fptrunc is governed by the "float:" override even though the operand is f64,
; so the subnormal result may be flushed to +0.0.

define float @src() denormal_fpenv(float: positivezero) {
; 0x3730000000000000 is 0x1p-140: a normal double, and a subnormal float
  %v = fptrunc double 0x3730000000000000 to float
  ret float %v
}

define float @tgt() denormal_fpenv(float: positivezero) {
  ret float 0.000000e+00
}
