@glb = global i8 0

define i8 @glb_alloca_noalias() {
  %ptr = alloca i8
  store i8 10, ptr %ptr
  store i8 20, ptr @glb
  %v = load i8, ptr %ptr
  ret i8 10
}
