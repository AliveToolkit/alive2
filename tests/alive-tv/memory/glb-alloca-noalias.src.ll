@glb = global i8 0

define i8 @glb_alloca_noalias() {
  %ptr = alloca i8
  store i8 10, i8* %ptr
  store i8 20, i8* @glb
  %v = load i8, i8* %ptr
  ret i8 %v
}
