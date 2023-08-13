@g = global {i8, i32} undef

; Non-padding is frozen
define i32 @src({i8, i32} noundef %cond) {
  store {i8, i32} %cond, ptr @g
  %v = load i8, ptr @g
  %v.fr = freeze i8 %v
  %c = icmp eq i8 %v.fr, 0
  br i1 %c, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt({i8, i32} noundef %cond) {
  store {i8, i32} %cond, ptr @g
  %v = load i8, ptr @g
  %c = icmp eq i8 %v, 0
  br i1 %c, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}
