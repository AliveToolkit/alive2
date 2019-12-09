@0 = global i8 0
@1 = global i32 0

define void @f() {
  %unused = load i8, i8* @0
  store i32 1, i32* @1
  ret void
}
