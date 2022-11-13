declare i8 @f(i8 returned)
declare ptr @f2(ptr returned)

define i8 @g(i8 %t) {
  %v = call i8 @f(i8 %t)
  ret i8 %v
}

define ptr @g2(ptr %p) {
  %p2 = call ptr @f2(ptr %p)
  ret ptr %p2
}

define i8 @g3() {
  %t = call i8 @f(i8 0)
  ret i8 %t
}

define void @g4(ptr %p) {
  %p2 = call ptr @f2(ptr %p)
  store i8 0, ptr %p2
  ret void
}

define i8 @g5(ptr readonly %p) {
  %p2 = call ptr @f2(ptr %p)
  %v = load i8, ptr %p2
  ret i8 %v
}

define void @g6(ptr readonly %p) {
  %p2 = call ptr @f2(ptr %p)
  store i8 0, ptr %p2 ; should be UB
  ret void
}
