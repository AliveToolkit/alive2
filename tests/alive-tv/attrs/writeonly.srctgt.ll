declare void @f() writeonly
@glb = global i8 0

define void @src() {
  store i8 1, i8* @glb
  call void @f()
  store i8 2, i8* @glb
  ret void
}

define void @tgt() {
  call void @f()
  store i8 2, i8* @glb
  ret void
}
