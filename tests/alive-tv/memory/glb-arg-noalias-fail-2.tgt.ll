@glb = global i8 0

define i8 @glb_arg_noalias(ptr %pptr) {
  %ptr = load ptr, ptr %pptr
  store i8 10, ptr %ptr
  store i8 20, ptr @glb
  ret i8 10
}

; ERROR: Value mismatch
