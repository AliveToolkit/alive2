@g = global i32 0
declare void @f(i32*)

define void @src() {
  %p = alloca i32
  call void @f(i32* byval %p)
  ret void
}

define void @tgt() {
  call void @f(i32* byval @g)
  ret void
}
