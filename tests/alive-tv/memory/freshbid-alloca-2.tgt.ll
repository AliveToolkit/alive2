; TEST-ARGS: -smt-to=9000

define i8 @freshbid_alloca(i8** %pptr) {
  %ptr = alloca i8
  %ptr0 = load i8*, i8** %pptr
  store i8 10, i8* %ptr
  store i8 20, i8* %ptr0
  ret i8 10
}
