declare i8 @f(i8 returned)
declare i8* @f2(i8* returned)

define i8 @g(i8 %t) {
  %v = call i8 @f(i8 %t)
  ret i8 %v
}

define i8* @g2(i8* %p) {
  %p2 = call i8* @f2(i8* %p)
  ret i8* %p2
}

define i8 @g3() {
  %t = call i8 @f(i8 0)
  ret i8 %t
}

define void @g4(i8* %p) {
  %p2 = call i8* @f2(i8* %p)
  store i8 0, i8* %p2
  ret void
}

define i8 @g5(i8* readonly %p) {
  %p2 = call i8* @f2(i8* %p)
  %v = load i8, i8* %p2
  ret i8 %v
}

define void @g6(i8* readonly %p) {
  %p2 = call i8* @f2(i8* %p)
  store i8 0, i8* %p2 ; should be UB
  ret void
}
