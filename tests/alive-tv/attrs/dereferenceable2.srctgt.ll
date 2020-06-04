define float @src(<4 x float>* dereferenceable(15) %p) {
  %bc = bitcast <4 x float>* %p to float*
  %r = load float, float* %bc, align 16
  ret float %r
}

define float @tgt(<4 x float>* dereferenceable(15) %p) {
  %bc = getelementptr inbounds <4 x float>, <4 x float>* %p, i64 0, i64 0
  %r = load float, float* %bc, align 16
  ret float %r
}
