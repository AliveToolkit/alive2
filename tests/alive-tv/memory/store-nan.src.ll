define half @f(ptr %ptr) {
  %f = fdiv half 0.0, 0.0
  store half %f, ptr %ptr
  %x = load half, ptr %ptr
  ret half %x
}
