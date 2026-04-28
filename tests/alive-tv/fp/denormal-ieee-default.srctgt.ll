; this transformation is OK since ieee is the default

define float @src(float %x) denormal_fpenv(ieee) {
  ret float %x
}

define float @tgt(float %x) {
  ret float %x
}
