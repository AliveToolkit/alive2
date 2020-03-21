; TEST-ARGS: -func=a -func=b

define i32 @a() {
  ret i32 0
}

define i32 @b() {
  ret i32 1
}

define i32 @c() {
  ret i32 3
}
