declare i8* @malloc(i64)
declare void @f(i8*)

define void @src(i64 %sz) {
  %p = call i8* @malloc(i64 %sz)
  %p.fr = freeze i8* %p
  call void @f(i8* %p.fr)
  ret void
}

define void @tgt(i64 %sz) {
  %p = call i8* @malloc(i64 %sz)
  call void @f(i8* %p)
  ret void
}
