; TEST-ARGS: -src-unroll=2 -tgt-unroll=2

declare i1 @use(i32)

define void @src() {
 entry:
  %t = alloca i32
  br label %loop

 loop:
  %v = load i32, ptr %t
  %c = call i1 @use(i32 %v)
  store i32 46, ptr %t
  store i32 42, ptr %t
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

; ERROR: Source is more defined than target
