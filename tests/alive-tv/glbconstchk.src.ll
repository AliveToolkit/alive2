@g = global i32 0

define i32 @f() {
  %v = load i32, i32* @g
  ret i32 %v
}

; ERROR: Global variable @g in source and target has different constness
