define i32 @src(i1 %c, ptr dereferenceable(4) %p) {
ENTRY:
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i32, ptr %p, align 1
  br label %EXIT
EXIT:
  %val = phi i32 [%v1, %A], [0, %ENTRY]
  ret i32 %val
}

define i32 @tgt(i1 %c, ptr dereferenceable(4) %p) {
  %v1 = load i32, ptr %p, align 1
  %val = select i1 %c, i32 %v1, i32 0
  ret i32 %val
}
