define void @src(i1 %c, ptr dereferenceable(4) align 4 %p) {
  br i1 %c, label %A, label %B
A:
  load i32, ptr %p, align 4
  ret void
B:
  ret void
}

define void @tgt(i1 %c, ptr dereferenceable(4) align 4%p) {
  load i32, ptr %p, align 4
  br i1 %c, label %A, label %B
A:
  ret void
B:
  ret void
}
