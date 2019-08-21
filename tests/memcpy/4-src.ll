declare void @memset(i8*, i8, i32)
declare void @memcpy(i8*, i8*, i32)

define i8 @f() {
  %p1 = alloca [16 x i8]
  %p2 = bitcast [16 x i8]* %p1 to i8*
  %p3 = alloca [16 x i8]
  %p4 = bitcast [16 x i8]* %p3 to i8*
  call void @memset(i8* %p2, i8 3, i32 16)
  call void @memcpy(i8* %p4, i8* %p2, i32 16)
  %p5 = getelementptr i8, i8* %p4 , i32 6
  %v = load i8, i8* %p5
  ret i8 %v
}
