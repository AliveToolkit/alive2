@g = global i32 0

define i32 @f() {
  %v = load i32, ptr @g
  ret i32 %v
}

; ERROR: Unsupported interprocedural transformation: global variable @g has different size in source and target (4 vs 1 bytes)
