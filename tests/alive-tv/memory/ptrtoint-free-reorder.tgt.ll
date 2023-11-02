target datalayout = "e-p:64:64"
define i64 @f() {
  %p = call noalias ptr @malloc(i64 10)
  %i = ptrtoint ptr %p to i64
  call void @free(ptr %p)
  ret i64 %i
}

declare void @free(ptr)
declare noalias ptr @malloc(i64)
