define i32 @f() {
  br label %header
header:
  br label %loop
loop:
  %i = phi i32 [0, %header], [%i, %loop]
  %c = icmp eq i32 %i, 10
  br i1 %c, label %exit, label %loop
exit:
  ret i32 10
}

; ERROR: Loops are not supported yet! Skipping function.
