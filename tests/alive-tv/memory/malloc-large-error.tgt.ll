target datalayout = "e-m:o-i64:64-n8:16:32:64"

define i8* @malloc_large() {
  ret i8* undef
}

declare i8* @malloc(i64)
