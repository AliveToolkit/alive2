define half @src(half %0) {
  %2 = fpext half %0 to float
  %3 = fptrunc float %2 to half
  ret half %3
}

define half @tgt(half %0) {
  ret half %0
}
