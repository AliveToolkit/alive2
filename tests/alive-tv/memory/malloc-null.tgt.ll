; target: 64 bits ptr addr
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i1 @malloc_null() {
  ret i1 false
}
