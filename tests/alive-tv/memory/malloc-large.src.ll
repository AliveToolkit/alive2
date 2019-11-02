target datalayout = "e-m:o-i64:64-n8:16:32:64"

define i8* @malloc_large() {
  %ret = call i8* @malloc(i64 -9223372036854775808)
  ret i8* %ret
}

declare i8* @malloc(i64)
