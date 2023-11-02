target datalayout="e-p:64:64"

define i8 @free_ub_doublefree() {
  %ptr = call noalias ptr @malloc(i64 4)
  ; %ptr == null?
  %i = ptrtoint ptr %ptr to i64
  %eq = icmp eq i64 %i, 0
  br i1 %eq, label %EXIT, label %DOUBLE_FREE
EXIT:
  ret i8 0
DOUBLE_FREE:
  ; double free is UB only when malloc returns non-null pointer.
  unreachable
}

declare noalias ptr @malloc(i64)
declare void @free(ptr)
