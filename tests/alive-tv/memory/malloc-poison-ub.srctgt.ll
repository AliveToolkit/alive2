declare i8* @malloc(i64)
declare i8* @calloc(i64, i64)

; Poison is a deferred UB, and the equivalent operation in C/C++ is immediate
; UB. Therefore, it is safe to assume that malloc(poison) is UB in LLVM IR too.

define void @src(i1 %cond) {
  %poison = add nuw i64 -1, -1
  br i1 %cond, label %MALLOC, label %CALLOC
MALLOC:
  call i8* @malloc(i64 %poison)
  ret void
CALLOC:
  call i8* @calloc(i64 %poison, i64 %poison)
  ret void
}

define void @tgt(i1 %cond) {
  unreachable
}
