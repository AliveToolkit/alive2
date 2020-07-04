define i16 @src(i1 %c) {
ENTRY:
  %p = call i16* @f()
  br i1 %c, label %A, label %EXIT
A:
  %v1 = load i16, i16* %p, align 1
  br label %EXIT
EXIT:
  %val = phi i16 [%v1, %A], [0, %ENTRY]
  ret i16 %val
}

define i16 @tgt(i1 %c) {
  %p = call i16* @f()
  %v1 = load i16, i16* %p, align 1
  %val = select i1 %c, i16 %v1, i16 0
  ret i16 %val
}

declare dereferenceable(2) i16* @f()
