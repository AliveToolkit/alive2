define i32 @src(i1 %c, i32* %p) {
ENTRY:
  call void @f(i32* %p)
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i32, i32* %p
  br label %EXIT
EXIT:
  %val = phi i32 [%v1, %A], [0, %ENTRY]
  ret i32 %val
}

define i32 @tgt(i1 %c, i32* %p) {
  call void @f(i32* %p)
  %v1 = load i32, i32* %p
  %val = select i1 %c, i32 %v1, i32 0
  ret i32 %val
}

declare void @f(i32* dereferenceable(4) %ptr)
