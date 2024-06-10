@vtable = constant ptr @foo

declare void @foo() memory(none)

define void @src(i1 %c) {
entry:
  br label %loop

loop:
  %fn = load ptr, ptr @vtable, align 8
  call void %fn()
  br i1 %c, label %exit, label %loop

exit:
  ret void
}

define void @tgt(i1 %c) {
entry:
  br label %loop

loop:
  call void @foo()
  br i1 %c, label %exit, label %loop

exit:
  ret void
}
