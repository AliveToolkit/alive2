target datalayout="e"

define i64 @src(ptr %p) {
  store <2 x float> <float 1.0, float 2.0>, ptr %p
  %i = load i64, ptr %p
  ret i64 %i
}

define i64 @tgt(ptr %p) {
	store <2 x float> <float 1.0, float 2.0>, ptr %p
	ret i64 4611686019492741120
}
