define half @f(half* %ptr) {
  %f = fdiv half 0.0, 0.0
  store half %f, half* %ptr
  %x = load half, half* %ptr
  ret half %x
}
