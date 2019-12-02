define float @f(float* %ptr) {
  %f = fdiv float 0.0, 0.0
  store float %f, float* %ptr
  ret float %f
}
