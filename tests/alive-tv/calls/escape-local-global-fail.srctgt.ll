@g = global i32 0
declare void @f(i32*)

define void @src() {
  %p = alloca i32
  call void @f(i32* %p)
  ret void
}

define void @tgt() {
  call void @f(i32* @g)
  ret void
}

; ERROR: Source is more defined than target
