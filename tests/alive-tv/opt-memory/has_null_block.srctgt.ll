; TEST-ARGS: -dbg

@x = global i32 0

define i32 @src() {
  %p = alloca i32
  %p2 = getelementptr i32, i32* %p, i32 0
  %y = load i32, i32* @x
  ret i32 %y
}

define i32 @tgt() {
  %p = alloca i32
  %p2 = getelementptr i32, i32* %p, i32 0
  %y = load i32, i32* @x
  ret i32 %y
}

; CHECK: has_null_block: 0
