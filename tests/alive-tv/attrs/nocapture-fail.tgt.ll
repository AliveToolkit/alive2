@x = global i8* null

define void @f(i8* nocapture %p) {
  store i8* %p, i8** @x
  ret void
}
