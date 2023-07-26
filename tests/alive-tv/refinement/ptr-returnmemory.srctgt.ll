; SKIP-IDENTITY

define ptr @src(i1 %cond, ptr %a, ptr %b, ptr %dummy) {
  call void @free(ptr %dummy) ; dummy free

  br i1 %cond, label %A, label %B
A:
  %c1 = call ptr @callee2(ptr %a)
  ret ptr %c1
B:
  %c2 = call ptr @callee2(ptr %b)
  ret ptr %c2
}

define ptr @tgt(i1 %cond, ptr %a, ptr %b, ptr %dummy) {
  call void @free(ptr %dummy) ; dummy free

  br i1 %cond, label %A, label %B
A:
  %c1 = call ptr @callee2(ptr %a)
  br label %EXIT
B:
  %c2 = call ptr @callee2(ptr %b)
  br label %EXIT
EXIT:
  %cc = phi ptr [%c1, %A], [%c2, %B]
  ret ptr %cc
}

declare ptr @callee2(ptr)
declare void @free(ptr)
