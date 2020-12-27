define i8 @src([12 x i8] %pathstr) {
  %path = alloca [12 x i8]
  store [12 x i8] %pathstr, [12 x i8]* %path

  %path.i8 = bitcast [12 x i8]* %path to i8*
  call i32 @access(i8* %path.i8, i32 0) ; F_OK
  %v = load i8, i8* %path.i8
  ret i8 %v
}

define i8 @tgt([12 x i8] %pathstr) {
  %path = alloca [12 x i8]
  store [12 x i8] %pathstr, [12 x i8]* %path

  %path.i8 = bitcast [12 x i8]* %path to i8*
  %v = load i8, i8* %path.i8

  call i32 @access(i8* %path.i8, i32 0) ; F_OK
  ret i8 %v
}

declare i32 @access(i8*, i32)
