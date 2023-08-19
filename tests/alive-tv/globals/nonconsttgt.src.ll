@var_10 = global i8 0, align 1

define void @test() {
entry:
  store i8 1, ptr @var_10, align 1
  ret void
}

; CHECK: Unsupported interprocedural transformation: non-constant global variable @var_11 is introduced in target
