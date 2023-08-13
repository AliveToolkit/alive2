; dereferenceable attribute does not guarantee that the pointer is unfreed

define i32 @src(i1 %c, ptr %p) {
ENTRY:
  call void @f(ptr %p)
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i32, ptr %p, align 1
  br label %EXIT
EXIT:
  %val = phi i32 [%v1, %A], [0, %ENTRY]
  ret i32 %val
}

define i32 @tgt(i1 %c, ptr %p) {
  call void @f(ptr %p)
  %v1 = load i32, ptr %p, align 1
  %val = select i1 %c, i32 %v1, i32 0
  ret i32 %val
}

; ERROR: Source is more defined than target
declare void @f(ptr dereferenceable(4) %ptr)
