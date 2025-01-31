target datalayout = "e-p:64:64:64"

define i32 @ub_null() {
  %p = alloca i32
  store i32 0, ptr %p
  %res = call i32 @memcmp(ptr %p, ptr null, i64 4) ; ub
  ret i32 %res
}

define i32 @ub_oob() {
  %p = alloca i32
  store i32 0, ptr %p
  %q = getelementptr i8, ptr %p, i64 4
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4) ; ub
  ret i32 %res
}

define i32 @ub_oob2() {
  %p = alloca i32
  store i32 0, ptr %p
  %q = getelementptr i8, ptr %p, i64 1
  %res = call i32 @memcmp(ptr %p, ptr %q, i64 4) ; ub!
  ret i32 %res
}

declare i32 @memcmp(ptr captures(none), ptr captures(none), i64)
