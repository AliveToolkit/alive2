define i8 @src([12 x i8] %pathstr) {
  %path = alloca [12 x i8]
  store [12 x i8] %pathstr, ptr %path

  call i32 @access(ptr %path, i32 0) ; F_OK
  %v = load i8, ptr %path
  ret i8 %v
}

define i8 @tgt([12 x i8] %pathstr) {
  %path = alloca [12 x i8]
  store [12 x i8] %pathstr, ptr %path

  %v = load i8, ptr %path

  call i32 @access(ptr %path, i32 0) ; F_OK
  ret i8 %v
}

declare i32 @access(ptr, i32)
