define half @src(half %x) {
  %s = fsub half -0.0, %x
  ret half %s
}

define half @tgt(half %x) {
  %s = fneg half %x
  ret half %s
}
