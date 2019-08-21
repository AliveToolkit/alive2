declare void @memcpy(i8*, i8*, i32)

define i8 @f() {
  %p1 = alloca [16 x i8]
  %p2 = bitcast [16 x i8]* %p1 to i8*
  %p3 = alloca [16 x i8]
  %p4 = bitcast [16 x i8]* %p3 to i8*
  store i8 3, i8* %p2
  call void @memcpy(i8* %p4, i8* %p2, i32 1)
  %v = load i8, i8* %p4
  ret i8 %v
}
