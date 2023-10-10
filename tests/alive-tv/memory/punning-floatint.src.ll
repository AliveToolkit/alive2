define float @float_int_punning(float %f) {
  %ptr = alloca i32
  %if = bitcast float %f to i32
  store i32 %if, ptr %ptr
  %f2 = load float, ptr %ptr
  ret float %f2
}
