; ERROR: Function attributes not refined

define float @src(float %x) denormal_fpenv(positivezero) {
  ret float %x
}

define float @tgt(float %x) {
  ret float %x
}
