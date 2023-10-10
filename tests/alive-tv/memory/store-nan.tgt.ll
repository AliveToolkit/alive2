define half @f(ptr %ptr) {
  %f = fdiv half 0.0, 0.0
  store half %f, ptr %ptr
  ret half %f
}
