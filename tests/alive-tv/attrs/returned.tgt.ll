declare i8 @f(i8 returned)
declare ptr @f2(ptr returned)

define i8 @g(i8 %t) {
  call i8 @f(i8 %t)
  ret i8 %t
}

define ptr @g2(ptr %p) {
  call ptr @f2(ptr %p)
  ret ptr %p
}

define i8 @g3() {
  call i8 @f(i8 0)
  ret i8 0
}

define void @g4(ptr %p) {
  call ptr @f2(ptr %p)
  store i8 0, ptr %p
  ret void
}

define i8 @g5(ptr readonly %p) {
  call ptr @f2(ptr %p)
  %v = load i8, ptr %p
  ret i8 %v
}

define void @g6(ptr readonly %p) {
  call ptr @f2(ptr %p)
  store i8 0, ptr %p ; should be UB
  ret void
}
