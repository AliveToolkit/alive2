declare i8* @malloc(i64)
@glb = global i8* null

define void @src() {
  %p = call i8* @malloc(i64 4)
  store i8* %p, i8** @glb
  ret void
}

define void @tgt() {
  %p = call i8* @malloc(i64 4)
  store i8* %p, i8** @glb
  ret void
}
