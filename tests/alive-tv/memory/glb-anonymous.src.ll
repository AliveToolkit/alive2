@0 = global i8 0
@1 = global i32 0

define void @f() {
  %unused = load i8, ptr @0
  store i32 1, ptr @1
  ret void
}
