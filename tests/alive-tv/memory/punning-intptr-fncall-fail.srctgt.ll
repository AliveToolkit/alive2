declare void @g(ptr)

define void @src(i64 %x) {
  %a = alloca i64
  store i64 %x, ptr %a
  %p = load ptr, ptr %a
  call void @g(ptr %p)
  ret void
}

define void @tgt(i64 %x) {
  %y = add i64 %x, 1
  %p = inttoptr i64 %y to ptr
  call void @g(ptr %p)
  ret void
}

; ERROR: Source is more defined than target
