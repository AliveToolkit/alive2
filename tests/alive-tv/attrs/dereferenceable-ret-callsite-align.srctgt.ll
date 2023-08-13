define i32 @src(i1 %c) {
ENTRY:
  %p = call dereferenceable(4) ptr @f()
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i32, ptr %p, align 4
  br label %EXIT
EXIT:
  %val = phi i32 [%v1, %A], [0, %ENTRY]
  ret i32 %val
}

define i32 @tgt(i1 %c) {
  %p = call dereferenceable(4) ptr @f()
  %v1 = load i32, ptr %p, align 4
  %val = select i1 %c, i32 %v1, i32 0
  ret i32 %val
}

declare ptr @f()

; ERROR: Source is more defined than target
