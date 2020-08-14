; TEST-ARGS: -disable-poison-input
; The option is not applied to padding
@g = global {i8, i32} undef

define i32 @src({i8, i32} noundef %cond) {
  store {i8, i32} %cond, {i8, i32}* @g
  %p = bitcast {i8, i32}* @g to i8*
  %p2 = getelementptr i8, i8* %p, i64 1
  %v = load i8, i8* %p2
  %v.fr = freeze i8 %v
  %c = icmp eq i8 %v.fr, 0
  br i1 %c, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt({i8, i32} noundef %cond) {
  store {i8, i32} %cond, {i8, i32}* @g
  %p = bitcast {i8, i32}* @g to i8*
  %p2 = getelementptr i8, i8* %p, i64 1
  %v = load i8, i8* %p2
  %c = icmp eq i8 %v, 0
  br i1 %c, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

; ERROR: Source is more defined than target
