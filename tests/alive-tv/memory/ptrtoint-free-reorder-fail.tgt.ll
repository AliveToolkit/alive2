target datalayout = "e-p:64:64"
define i64 @f() {
  %p = call noalias ptr @malloc(i64 10)
  %i0 = ptrtoint ptr %p to i64
  %i = add i64 %i0, 1
  call void @free(ptr %p)
  ret i64 %i
}

declare void @free(ptr)
declare noalias ptr @malloc(i64)
