@g = global {i8, i32} undef
declare noundef {i8, i32} @f()

define i32 @src() {
  %out = call {i8, i32} @f()
  store {i8, i32} %out, {i8, i32}* @g
  %p = bitcast {i8, i32}* @g to i8*
  %p2 = getelementptr i8, i8* %p, i64 1

  %v = load i8, i8* %p2
  %cond = icmp eq i8 %v, 10
  %f = freeze i1 %cond
  br i1 %f, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt() {
  %out = call {i8, i32} @f()
  store {i8, i32} %out, {i8, i32}* @g
  %p = bitcast {i8, i32}* @g to i8*
  %p2 = getelementptr i8, i8* %p, i64 1

  %v = load i8, i8* %p2
  %cond = icmp eq i8 %v, 10
  br i1 %cond, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

; ERROR: Source is more defined than target
