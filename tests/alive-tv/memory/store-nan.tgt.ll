define half @f(half* %ptr) {
  %f = fdiv half 0.0, 0.0
  store half %f, half* %ptr
  ret half %f
}
