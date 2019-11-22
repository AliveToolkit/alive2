; TEST-ARGS: -disable-undef-input
; -disable-undef-input is here to relax timeout
@glb = global i8 0

define i8 @glb_arg_noalias(i8** %pptr) {
  %ptr = load i8*, i8** %pptr
  store i8 10, i8* %ptr
  store i8 20, i8* @glb
  %v = load i8, i8* %ptr
  ret i8 %v
}

; ERROR: Value mismatch
