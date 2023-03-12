; SKIP-IDENTITY

define i8* @src(i1 %cond, i8* %a, i8* %b, i8* %dummy) {
  call void @free(i8* %dummy) ; dummy free

  br i1 %cond, label %A, label %B
A:
  %c1 = call i8* @callee2(i8* %a)
  ret i8* %c1
B:
  %c2 = call i8* @callee2(i8* %b)
  ret i8* %c2
}

define i8* @tgt(i1 %cond, i8* %a, i8* %b, i8* %dummy) {
  call void @free(i8* %dummy) ; dummy free

  br i1 %cond, label %A, label %B
A:
  %c1 = call i8* @callee2(i8* %a)
  br label %EXIT
B:
  %c2 = call i8* @callee2(i8* %b)
  br label %EXIT
EXIT:
  %cc = phi i8* [%c1, %A], [%c2, %B]
  ret i8* %cc
}


declare i8* @callee2(i8*)
declare void @free(i8*)
