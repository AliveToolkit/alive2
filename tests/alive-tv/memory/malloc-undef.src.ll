; ERROR: Value mismatch

define i8 @malloc_undef() {
  %ptr = call i8* @malloc(i64 undef)
  %addr = ptrtoint i8* %ptr to i64
  %isnull = icmp eq i64 %addr, 0
  br i1 %isnull, label %EXIT, label %STORE
EXIT:
  ret i8 0
STORE:
  ret i8 1
}

declare i8* @malloc(i64)
