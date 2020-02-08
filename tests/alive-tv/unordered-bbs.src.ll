declare i1 @f()

define void @g() {
  br label %AA
BB:
  br i1 %c2, label %AA, label %EXIT
AA:
  %c2 = call i1 @f()
  br i1 %c2, label %BB, label %EXIT
EXIT:
  ret void
}
