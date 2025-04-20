@p1 = external global ptr, align 8
@p2 = external global ptr, align 8

define void @src() {
  %p3 = alloca i64, align 8
  %n0 = load ptr, ptr @p1, align 8
  %n1 = load ptr, ptr %n0, align 8
  %n2 = load ptr, ptr @p2, align 8
  %n3 = load ptr, ptr %n2, align 8
  store ptr %n1, ptr %n3, align 8
  store i64 0, ptr %p3, align 8
  ret void
}

define void @tgt() {
  %n0 = load ptr, ptr @p1, align 8
  %n1 = load ptr, ptr %n0, align 8
  %n2 = load ptr, ptr @p2, align 8
  %n3 = load ptr, ptr %n2, align 8
  store ptr %n1, ptr %n3, align 8
  ret void
}
