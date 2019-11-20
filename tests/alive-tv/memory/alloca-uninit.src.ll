; TEST-ARGS: -smt-to=9000

define i32 @f() {
  %p = alloca i32
  %v = load i32, i32* %p
  %res0 = add i32 %v, %v
  %res = add i32 %res0, 1
  ret i32 %res
}
