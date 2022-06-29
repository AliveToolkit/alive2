; ERROR: Mismatch in memory

@glb = internal global i8 0

define void @f() {
  store i8 0, ptr @glb
  ret void
}
