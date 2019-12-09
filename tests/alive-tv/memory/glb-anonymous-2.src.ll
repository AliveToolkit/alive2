@0 = constant i32 1
@x = constant i32 2
@1 = constant i32 3

define i32 @f() {
  %a = load i32, i32* @0
  %b = load i32, i32* @x
  %c = load i32, i32* @1

  %p = mul i32 100, %a
  %q = mul i32 10, %b

  %res0 = add i32 %p, %q
  %res = add i32 %res0, %c
  ret i32 %res
}
