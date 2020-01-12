define void @f() {
  br label %loop
loop:
  ; It is UB to have infinite loop without side effect in C, but
  ; it is not clear it also applies to LLVM.
  ; Here is an unknown function call g() inside this loop, so it doesn't apply
  ; here.
  call void @g()
  br label %loop
}

declare void @g()
