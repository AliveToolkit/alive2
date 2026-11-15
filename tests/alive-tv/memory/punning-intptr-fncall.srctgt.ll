; A pointer loaded from integer bytes is refined by inttoptr of the integer
; also when passed to a call
declare void @g(ptr)
declare [2 x i64] @f()

define void @src(i64 %x) {
  %a = alloca i64
  store i64 %x, ptr %a
  %p = load ptr, ptr %a
  call void @g(ptr %p)
  ret void
}

define void @tgt(i64 %x) {
  %p = inttoptr i64 %x to ptr
  call void @g(ptr %p)
  ret void
}

define void @src2() {
  %a = alloca [2 x i64], align 8
  %r = call [2 x i64] @f()
  store [2 x i64] %r, ptr %a, align 8
  %p = load ptr, ptr %a, align 8
  call void @g(ptr %p)
  ret void
}

define void @tgt2() {
  %r = call [2 x i64] @f()
  %r0 = extractvalue [2 x i64] %r, 0
  %p = inttoptr i64 %r0 to ptr
  call void @g(ptr %p)
  ret void
}
