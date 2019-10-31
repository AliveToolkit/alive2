@g = global i32 0

define i32 @f() {
  %v = load i32, i32* @g
  ret i32 %v
}

; ERROR: Can't verify this because global variable @g is const in target but not in source
