@x = global ptr null
@y = global i8 0

define void @f1() {
  store ptr null, ptr @x
  ret void
}
