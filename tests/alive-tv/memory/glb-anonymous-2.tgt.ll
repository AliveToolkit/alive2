@0 = constant i32 1
@x = constant i32 2
@1 = constant i32 3

define i32 @f() {
  ; reordered
  %b = load i32, ptr @x
  %c = load i32, ptr @1
  %a = load i32, ptr @0

  ret i32 123
}
