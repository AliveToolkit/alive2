; The denormal mode is per-type, and the flushing of an operation's result is
; governed by the mode of the *result* type, not of its operands.  Here the
; "float:" override applies only to f32, so the f16 result of this fptrunc
; follows the base ieee mode and must not be flushed.

define half @src() denormal_fpenv(float: preservesign) {
  ret half 0xH0001
}

define half @tgt() denormal_fpenv(float: preservesign) {
; 0x3E70000000000000 is 0x1p-24: a normal float, and the smallest subnormal half
  %v = fptrunc float 0x3E70000000000000 to half
  ret half %v
}
