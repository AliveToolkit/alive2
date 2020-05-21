define i32 @src(i1 %c) {
ENTRY:
  %p = call dereferenceable(4) i32* @f()
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i32, i32* %p
  br label %EXIT
EXIT:
  %val = phi i32 [%v1, %A], [0, %ENTRY]
  ret i32 %val
}

define i32 @tgt(i1 %c) {
  %p = call dereferenceable(4) i32* @f()
  %v1 = load i32, i32* %p
  %val = select i1 %c, i32 %v1, i32 0
  ret i32 %val
}

declare i32* @f()
