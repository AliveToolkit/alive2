; To detect a bug from this, infinite loop should be supported

declare i1 @use(i32)

define void @src() {
 entry:
  %t = alloca i32
  br label %loop

 loop:
  %v = load i32, i32* %t
  %c = call i1 @use(i32 %v)
  store i32 46, i32* %t
  store i32 42, i32* %t
  br i1 %c, label %loop, label %exit

 exit:
  ret void
}

define void @tgt() {
entry:
  br label %loop

loop:                                             ; preds = %loop, %entry
  %c = tail call i1 @use(i32 undef)
  br i1 %c, label %loop, label %exit

exit:                                             ; preds = %loop
  ret void
}
