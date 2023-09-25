; TEST-ARGS: -dbg

define void @src() {
entry:
  %p = alloca i64, align 4
  br label %loop

loop:
  %p2 = phi ptr [ %p, %entry ], [ %p3, %loop ]
  store i16 0, ptr %p2, align 1
  %p3 = getelementptr i8, ptr %p2, i64 2
  %cond = call i1 @cond()
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}

define void @tgt() {
entry:
  %p = alloca i64, align 4
  br label %loop

loop:
  %p2 = phi ptr [ %p, %entry ], [ %p3, %loop ]
  store i16 0, ptr %p2, align 2
  %p3 = getelementptr i8, ptr %p2, i64 2
  %cond = call i1 @cond()
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}

declare i1 @cond()

; CHECK: bits_byte: 16
