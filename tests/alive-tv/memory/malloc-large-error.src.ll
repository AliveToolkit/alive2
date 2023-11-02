target datalayout = "e-m:o-i64:64-n8:16:32:64"

define ptr @malloc_large() {
  %ret = call ptr @malloc(i64 -9223372036854775808)
  ret ptr %ret
}

; ERROR: Target's return value is more undefined

declare ptr @malloc(i64)
