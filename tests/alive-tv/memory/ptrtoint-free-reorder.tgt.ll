target datalayout = "e-p:64:64"
define i64 @f() {
  %p = call noalias i8* @malloc(i64 10)
  %i = ptrtoint i8* %p to i64
  call void @free(i8* %p)
  ret i64 %i
}

declare void @free(i8*)
declare noalias i8* @malloc(i64)
