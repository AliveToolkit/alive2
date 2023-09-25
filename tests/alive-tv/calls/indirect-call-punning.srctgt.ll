declare ptr @ret_ptr()
declare i32 @ret_i32()
declare i8 @in_i64(i64)
declare i8 @in_ptr(ptr)

define i64 @src_1() {
  %v = call i64 @ret_ptr()
  ret i64 %v
}

define i64 @tgt_1() {
  unreachable
}

define ptr @src_2() {
  %v = call ptr @ret_i32()
  ret ptr %v
}

define ptr @tgt_2() {
  unreachable
}

define i8 @src_3() {
  %v = call i8 @in_i64(ptr null)
  ret i8 %v
}

define i8 @tgt_3() {
  unreachable
}

define i8 @src_4() {
  %v = call i8 @in_ptr(i64 3)
  ret i8 %v
}

define i8 @tgt_4() {
  unreachable
}
