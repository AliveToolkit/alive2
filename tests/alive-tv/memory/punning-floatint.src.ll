define float @float_int_punning(float %f) {
  %ptr = alloca i32
  %if = bitcast float %f to i32
  store i32 %if, i32* %ptr
  %fptr = bitcast i32* %ptr to float*
  %f2 = load float, float* %fptr
  ret float %f2
}
