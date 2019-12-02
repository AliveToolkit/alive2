define float @f(float* %ptr) {
  %f = fdiv float 0.0, 0.0
  store float %f, float* %ptr
  %x = load float, float* %ptr
  ret float %x
}
