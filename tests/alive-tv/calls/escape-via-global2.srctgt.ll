@glb = global i8* null
declare void @f()

define void @src() {
  %p = alloca i8
  store i8* %p, i8** @glb
  call void @f()
  ret void
}

define void @tgt() {
  %p = alloca i8
  store i8* %p, i8** @glb
  call void @f()
  ret void
}
