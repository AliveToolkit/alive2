; TEST-ARGS: -smt-to=15000 -disable-undef-input
; To resolve timeout error, it is assumed that input %pptr is never an undef pointer

define i8 @freshbid_alloca(i8** %pptr) {
  %ptr = alloca i8
  %ptr0 = load i8*, i8** %pptr
  store i8 10, i8* %ptr
  store i8 20, i8* %ptr0
  %v = load i8, i8* %ptr
  ret i8 %v
}

; Couldn't figure out how to make this test work
; Leave it as XFAIL.
; XFAIL: Timeout
