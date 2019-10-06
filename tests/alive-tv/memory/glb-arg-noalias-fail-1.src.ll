@glb = global i8 0

define i8 @glb_arg_noalias(i8* %ptr) {
  store i8 10, i8* %ptr
  store i8 20, i8* @glb
  %v = load i8, i8* %ptr
  ret i8 %v
}

; ERROR: Value mismatch
