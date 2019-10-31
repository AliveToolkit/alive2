@g = constant i32 0

define i32 @f() {
  %v = load i32, i32* @g
  ret i32 %v
}

; ERROR: Transformation is incorrect because global variable @g is const in source but not in target
