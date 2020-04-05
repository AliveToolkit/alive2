target datalayout="e"

define i64 @src(<2 x float>* %p) {
  store <2 x float> <float 1.0, float 2.0>, <2 x float>* %p
  %p2 = bitcast <2 x float>* %p to i64*
  %i = load i64, i64* %p2
  ret i64 %i
}

define i64 @tgt(<2 x float>* %p) {
	store <2 x float> <float 1.0, float 2.0>, <2 x float>* %p
	ret i64 4611686019492741120
}
