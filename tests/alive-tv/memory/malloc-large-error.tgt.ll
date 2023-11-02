target datalayout = "e-m:o-i64:64-n8:16:32:64"

define ptr @malloc_large() {
  ret ptr undef
}

declare ptr @malloc(i64)
