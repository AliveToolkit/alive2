declare void @f(i8*)

define void @src() {
  %p = alloca i8
  store i8 1, i8* %p
  call void @f(i8* %p)
  ret void
}

define void @tgt() {
  %p = alloca i8
  store i8 2, i8* %p
  call void @f(i8* %p)
  ret void
}

; ERROR: Source is more defined than target
