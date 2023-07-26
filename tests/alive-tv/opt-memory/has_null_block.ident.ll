; TEST-ARGS: -dbg

@x = global i32 0

define i32 @f() {
  %p = alloca i32
  %p2 = getelementptr i32, ptr %p, i32 0
  %y = load i32, ptr @x
  ret i32 %y
}

; CHECK: has_null_block: 0
