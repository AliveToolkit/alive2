; ERROR: Source is more defined than target

declare i32 @g(ptr byval(i32))

define i32 @src() {
entry:
  %p = alloca i32, align 4
  %q = alloca i32, align 4
  store i32 1, ptr %p, align 4
  store i32 2, ptr %q, align 4
  %r = call i32 @g(ptr byval(i32) %p)
  ret i32 %r
}

define i32 @tgt() {
entry:
  %p = alloca i32, align 4
  %q = alloca i32, align 4
  store i32 1, ptr %p, align 4
  store i32 2, ptr %q, align 4
  %r = call i32 @g(ptr byval(i32) %q)
  ret i32 %r
}
