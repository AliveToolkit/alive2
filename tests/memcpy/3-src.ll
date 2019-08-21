declare void @memcpy(i8*, i8*, i32)
declare void @memset(i8*, i8, i32)

define i8 @f() {
  %p1 = alloca [16 x i8]
  %p2 = bitcast [16 x i8]* %p1 to i8*
  call void @memset(i8* %p2, i8 3, i32 2)
  %p3 = getelementptr i8, i8* %p2 , i32 2
  call void @memcpy(i8* %p3, i8* %p2, i32 3)
  %v = load i8, i8* %p3
  ret i8 %v
}
