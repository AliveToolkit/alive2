@var_10 = global i8 0, align 1
@var_11 = constant i8 0, align 1

define void @test() {
entry:
  store i8 1, i8* @var_10, align 1
  %q = load i8, i8* @var_11, align 1
  ret void
}

