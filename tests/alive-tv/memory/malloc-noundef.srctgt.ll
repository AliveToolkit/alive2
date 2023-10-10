declare ptr @malloc(i64)
declare void @f(ptr)

define void @src(i64 %sz) {
  %p = call ptr @malloc(i64 %sz)
  %p.fr = freeze ptr %p
  call void @f(ptr %p.fr)
  ret void
}

define void @tgt(i64 %sz) {
  %p = call ptr @malloc(i64 %sz)
  call void @f(ptr %p)
  ret void
}
