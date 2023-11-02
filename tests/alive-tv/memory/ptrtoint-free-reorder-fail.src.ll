target datalayout = "e-p:64:64"
define i64 @f() {
  %p = call noalias ptr @malloc(i64 10)
  %c = icmp eq ptr %p, null
  br i1 %c, label %EXIT, label %BB
EXIT:
  ret i64 1
BB:
  call void @free(ptr %p)
  %i = ptrtoint ptr %p to i64
  ret i64 %i
}

; ERROR: Value mismatch
declare void @free(ptr)
declare noalias ptr @malloc(i64)
